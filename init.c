#include "acx_debug.h"

#include "acx.h"
#include "usb.h"
#include "merge.h"
#include "mem.h"
#include "pci.h"
#include "cmd.h"
#include "ie.h"
#include "init.h"
#include "cardsetting.h"

#define ACX111_PERCENT(percent) ((percent)/5)

/* Probably a number of acx's intermediate buffers for USB transfers,
 * not to be confused with number of descriptors in tx/rx rings
 * (which are not directly accessible to host in USB devices)
 */
#define USB_RX_CNT 10
#define USB_TX_CNT 10

static int acx_init_max_template_generic(acx_device_t *adev, unsigned int len,
				unsigned int cmdarg)
{
	int res;
	union {
		acx_template_nullframe_t null;
		acx_template_beacon_t b;
		acx_template_tim_t tim;
		acx_template_probereq_t preq;
		acx_template_proberesp_t presp;
	} templ;

	memset(&templ, 0, len);
	templ.null.size = cpu_to_le16(len - 2);
	res = acx_issue_cmd(adev, cmdarg, &templ, len);
	return res;
}

static int acx_init_max_null_data_template(acx_device_t *adev)
{
	/* OW hh version:
	 * issue_cmd(cmd:cmd,buflen:26,timeout:50ms,type:0x0018)
	 * mac80211 version: issue_cmd: begin:
	 * (cmd:cmd,buflen:32,timeout:50ms,type:0x001E)
	 *
	 * diff with hh is struct ieee80211_hdr included in
	 * acx_template_nullframe_t, which is bigger, thus size if
	 * bigger
	 */
	return acx_init_max_template_generic(adev,
		sizeof(acx_template_nullframe_t), ACX1xx_CMD_CONFIG_NULL_DATA);
}


static int acx_init_max_beacon_template(acx_device_t *adev)
{
	return acx_init_max_template_generic(adev,
					sizeof(acx_template_beacon_t),
					ACX1xx_CMD_CONFIG_BEACON);
}

static int acx_init_max_tim_template(acx_device_t *adev)
{
	return acx_init_max_template_generic(adev,
					sizeof(acx_template_tim_t),
					ACX1xx_CMD_CONFIG_TIM);
}

static int acx_init_max_probe_response_template(acx_device_t *adev)
{
	return acx_init_max_template_generic(adev,
					sizeof(acx_template_proberesp_t),
					ACX1xx_CMD_CONFIG_PROBE_RESPONSE);
}

static int acx_init_max_probe_request_template(acx_device_t *adev)
{
	return acx_init_max_template_generic(adev,
					sizeof(acx_template_probereq_t),
					ACX1xx_CMD_CONFIG_PROBE_REQUEST);
}

/*
 * acx_s_init_packet_templates()
 *
 * NOTE: order is very important here, to have a correct memory
 * layout!  init templates: max Probe Request (station mode), max NULL
 * data, max Beacon, max TIM, max Probe Response.
 */
static int acx_init_packet_templates(acx_device_t *adev)
{
	acx_ie_memmap_t mm;	/* ACX100 only */
	int result = NOT_OK;



	log(L_DEBUG | L_INIT, "initializing max packet templates\n");

	if (OK != acx_init_max_probe_request_template(adev))
		goto failed;

	if (OK != acx_init_max_null_data_template(adev))
		goto failed;

	if (OK != acx_init_max_beacon_template(adev))
		goto failed;

	if (OK != acx_init_max_tim_template(adev))
		goto failed;

	if (OK != acx_init_max_probe_response_template(adev))
		goto failed;

	if (IS_ACX111(adev)) {
		/* ACX111 doesn't need the memory map magic below, and
		 * the other templates will be set later (acx_start) */
		result = OK;
		goto success;
	}

	/* ACX100 will have its TIM template set,
	 * and we also need to update the memory map */

	if (OK != acx_set_tim_template(adev, NULL, 0))
		goto failed_acx100;

	log(L_DEBUG, "sizeof(memmap) = %d bytes\n", (int)sizeof(mm));

	if (OK != acx_interrogate(adev, &mm, ACX1xx_IE_MEMORY_MAP))
		goto failed_acx100;

	mm.QueueStart = cpu_to_le32(le32_to_cpu(mm.PacketTemplateEnd) + 4);
	if (OK != acx_configure(adev, &mm, ACX1xx_IE_MEMORY_MAP))
		goto failed_acx100;

	result = OK;
	goto success;

failed_acx100:
	log(L_DEBUG | L_INIT,
	    /* "cb=0x%X\n" */
	    "acx: ACXMemoryMap:\n"
	    "acx: .CodeStart=0x%X\n"
	    "acx: .CodeEnd=0x%X\n"
	    "acx: .WEPCacheStart=0x%X\n"
	    "acx: .WEPCacheEnd=0x%X\n"
	    "acx: .PacketTemplateStart=0x%X\n"
	    "acx: .PacketTemplateEnd=0x%X\n",
	    /* len, */
	    le32_to_cpu(mm.CodeStart),
	    le32_to_cpu(mm.CodeEnd),
	    le32_to_cpu(mm.WEPCacheStart),
	    le32_to_cpu(mm.WEPCacheEnd),
	    le32_to_cpu(mm.PacketTemplateStart),
	    le32_to_cpu(mm.PacketTemplateEnd));

failed:
	pr_info("%s: FAILED\n", wiphy_name(adev->ieee->wiphy));

success:

	return result;
}

/*
 * acx111_s_create_dma_regions
 *
 * Note that this fn messes heavily with hardware, but we cannot
 * lock it (we need to sleep). Not a problem since IRQs can't happen
 */
static int acx111_create_dma_regions(acx_device_t *adev)
{
	int i;

	struct acx111_ie_memoryconfig memconf;
	struct acx111_ie_queueconfig queueconf;
	u32 rx_queue_start;
	u32 tx_queue_start[ACX111_NUM_HW_TX_QUEUES];

	adev->num_hw_tx_queues = ACX111_NUM_HW_TX_QUEUES;

	/* Calculate memory positions and queue sizes */

	/* Set up our host descriptor pool + data pool */
	if (IS_PCI(adev) || IS_MEM(adev)) {
		if (OK != acx_create_hostdesc_queues(adev, ACX111_NUM_HW_TX_QUEUES))
			goto fail;
	}

	memset(&memconf, 0, sizeof(memconf));
	/* the number of STAs (STA contexts) to support
	 ** NB: was set to 1 and everything seemed to work nevertheless... */
	memconf.no_of_stations = 1; //cpu_to_le16(ARRAY_SIZE(adev->sta_list));

	/* specify the memory block size. Default is 256 */
	memconf.memory_block_size = cpu_to_le16(adev->memblocksize);
	/* let's use 50%/50% for tx/rx (specify percentage, units of 5%) */
	memconf.tx_rx_memory_block_allocation = ACX111_PERCENT(50);
	/* set the count of our queues
	 ** NB: struct acx111_ie_memoryconfig shall be modified
	 ** if we ever will switch to more than one rx and/or tx queue */
	memconf.count_rx_queues = 1;
	memconf.count_tx_queues = ACX111_NUM_HW_TX_QUEUES;
	/* 0 == Busmaster Indirect Memory Organization, which is what
	 * we want (using linked host descs with their allocated mem).
	 * 2 == Generic Bus Slave */
	/* done by memset: memconf.options = 0; */
	/* let's use 25% for fragmentations and 75% for frame transfers
	 * (specified in units of 5%) */
	memconf.fragmentation = ACX111_PERCENT(75);
	/* Rx descriptor queue config */
	memconf.rx_queue1_count_descs = RX_CNT;
	memconf.rx_queue1_type = 7;	/* must be set to 7 */

	/* done by memset: memconf.rx_queue1_prio = 0; low prio */
	if (IS_PCI(adev)) {
		#if defined(CONFIG_ACX_MAC80211_PCI)
		memconf.rx_queue1_host_rx_start =
		    cpu2acx(adev->hw_rx_queue.host.phy);
		#endif
	}
	else if (IS_MEM(adev)) {
		#if defined(CONFIG_ACX_MAC80211_MEM)
		memconf.rx_queue1_host_rx_start =
			cpu2acx(adev->hw_rx_queue.host.phy);
		#endif
	}

	/* Tx descriptor queue config */
	for (i = 0; i < ACX111_NUM_HW_TX_QUEUES; i++) {
		memconf.tx_queue[i].count_descs = TX_CNT;

		// TODO check if prio if up- or downwards
		/* done by memset: memconf.tx_queue1_attributes = 0; lowest priority */
		memconf.tx_queue[i].attributes = ACX111_NUM_HW_TX_QUEUES - 1 - i;
	}


	if (OK != acx_configure(adev, &memconf, ACX111_IE_MEMORY_CONFIG_OPTIONS))
		goto fail;

	memset(&queueconf, 0, sizeof(queueconf));
	acx_interrogate(adev, &queueconf, ACX111_IE_QUEUE_CONFIG);

	rx_queue_start = le32_to_cpu(queueconf.rx1_queue_address);

	for (i=0; i<ACX111_NUM_HW_TX_QUEUES; i++)
		tx_queue_start[i] = le32_to_cpu(queueconf.tx_queue[i].address);

	log(L_INIT,
	    "Queue head: len=%u, tx_memory_block_address=%X, rx_memory_block_address=%X\n",
	    le16_to_cpu(queueconf.len),
	    le32_to_cpu(queueconf.tx_memory_block_address),
	    le32_to_cpu(queueconf.rx_memory_block_address));
	log(L_INIT, "Queue head: rx_queue_start=%X\n", rx_queue_start);
	for (i=0; i<ACX111_NUM_HW_TX_QUEUES; i++)
		log(L_INIT, "Queue head: tx_queue_start[%d]=%X\n", i, tx_queue_start[i]);

	acx_create_desc_queues(adev, rx_queue_start, tx_queue_start,
	        ACX111_NUM_HW_TX_QUEUES);

	return OK;

fail:
	if (IS_PCI(adev) || IS_MEM(adev))
		acx_free_desc_queues(adev);

	return NOT_OK;
}


/*
 * acx100_s_init_wep
 *
 * FIXME: this should probably be moved into the new card settings
 * management, but since we're also modifying the memory map layout
 * here due to the WEP key space we want, we should take care...
 */
static int acx100_init_wep(acx_device_t * adev)
{
	/* acx100_ie_wep_options_t options;
	 * ie_dot11WEPDefaultKeyID_t dk;
	 */
	acx_ie_memmap_t pt;
	int res = NOT_OK;



	if (OK != acx_interrogate(adev, &pt, ACX1xx_IE_MEMORY_MAP))
		goto fail;

	log(L_DEBUG, "CodeEnd:%X\n", pt.CodeEnd);

	pt.WEPCacheStart = cpu_to_le32(le32_to_cpu(pt.CodeEnd) + 0x4);
	pt.WEPCacheEnd = cpu_to_le32(le32_to_cpu(pt.CodeEnd) + 0x4);

	if (OK != acx_configure(adev, &pt, ACX1xx_IE_MEMORY_MAP))
		goto fail;

/* OW: This disables WEP by not configuring the WEP cache and leaving
 * WEPCacheStart=WEPCacheEnd.
 *
 * When doing the crypto by mac80211 it is required, that the acx is
 * not doing any WEP crypto himself. Otherwise TX "WEP key not found"
 * errors occure.
 *
 * By disabling WEP using WEPCacheStart=WEPCacheStart the acx not
 * trying any own crypto anymore. All crypto (including WEP) is pushed
 * to mac80211 for the moment.
 *
 */
#if 0

	/* let's choose maximum setting: 4 default keys, plus 10 other keys: */
	options.NumKeys = cpu_to_le16(DOT11_MAX_DEFAULT_WEP_KEYS + 10);
	options.WEPOption = 0x00;

	log(L_ASSOC, "writing WEP options\n");
	acx_configure(adev, &options, ACX100_IE_WEP_OPTIONS);

	acx100_set_wepkey(adev);

	if (adev->wep_keys[adev->wep_current_index].size != 0) {
		log(L_ASSOC, "setting active default WEP key number: %d\n",
			adev->wep_current_index);
		dk.KeyID = adev->wep_current_index;
		acx_configure(adev, &dk,
			ACX1xx_IE_DOT11_WEP_DEFAULT_KEY_SET);	/* 0x1010 */
	}
	/* FIXME!!! wep_key_struct is filled nowhere! But adev
	 * is initialized to 0, and we don't REALLY need those keys either */
/*		for (i = 0; i < 10; i++) {
		if (adev->wep_key_struct[i].len != 0) {
			MAC_COPY(wep_mgmt.MacAddr, adev->wep_key_struct[i].addr);
			wep_mgmt.KeySize = cpu_to_le16(adev->wep_key_struct[i].len);
			memcpy(&wep_mgmt.Key, adev->wep_key_struct[i].key, le16_to_cpu(wep_mgmt.KeySize));
			wep_mgmt.Action = cpu_to_le16(1);
			log(L_ASSOC, "writing WEP key %d (len %d)\n", i, le16_to_cpu(wep_mgmt.KeySize));
			if (OK == acx_s_issue_cmd(adev, ACX1xx_CMD_WEP_MGMT, &wep_mgmt, sizeof(wep_mgmt))) {
				adev->wep_key_struct[i].index = i;
			}
		}
	}
*/

	/* now retrieve the updated WEPCacheEnd pointer... */
	if (OK != acx_interrogate(adev, &pt, ACX1xx_IE_MEMORY_MAP)) {
		pr_info("%s: ACX1xx_IE_MEMORY_MAP read #2 FAILED\n",
		       wiphy_name(adev->ieee->wiphy));
		goto fail;
	}
#endif

	/* ...and tell it to start allocating templates at that location */
	/* (no endianness conversion needed) */
	pt.PacketTemplateStart = pt.WEPCacheEnd;

	if (OK != acx_configure(adev, &pt, ACX1xx_IE_MEMORY_MAP)) {
		pr_info("%s: ACX1xx_IE_MEMORY_MAP write #2 FAILED\n",
		       wiphy_name(adev->ieee->wiphy));
		goto fail;
	}
	res = OK;

fail:

	return res;
}

static int acx100_init_memory_pools(acx_device_t *adev,
				const acx_ie_memmap_t *mmt)
{
	acx100_ie_memblocksize_t MemoryBlockSize;
	acx100_ie_memconfigoption_t MemoryConfigOption;
	int TotalMemoryBlocks;
	int RxBlockNum;
	int TotalRxBlockSize;
	int TxBlockNum;
	int TotalTxBlockSize;



	/* Let's see if we can follow this:
	   first we select our memory block size (which I think is
	   completely arbitrary) */
	MemoryBlockSize.size = cpu_to_le16(adev->memblocksize);

	/* Then we alert the card to our decision of block size */
	if (OK != acx_configure(adev, &MemoryBlockSize, ACX100_IE_BLOCK_SIZE))
		goto bad;

	/* We figure out how many total blocks we can create, using
	   the block size we chose, and the beginning and ending
	   memory pointers, i.e.: end-start/size */
	TotalMemoryBlocks =
	    (le32_to_cpu(mmt->PoolEnd) -
	     le32_to_cpu(mmt->PoolStart)) / adev->memblocksize;

	log(L_ANY, "TotalMemoryBlocks=%u (%u bytes)\n",
	    TotalMemoryBlocks, TotalMemoryBlocks * adev->memblocksize);

	/* MemoryConfigOption.DMA_config bitmask:
	   access to ACX memory is to be done:
	   0x00080000   using PCI conf space?!
	   0x00040000   using IO instructions?
	   0x00000000   using memory access instructions
	   0x00020000   using local memory block linked list (else what?)
	   0x00010000   using host indirect descriptors (else host must access ACX memory?)
	 */
	if (IS_PCI(adev)) {
		#if defined(CONFIG_ACX_MAC80211_PCI)
		MemoryConfigOption.DMA_config = cpu_to_le32(0x30000);
		/* Declare start of the Rx host pool */
		MemoryConfigOption.pRxHostDesc =
		    cpu2acx(adev->hw_rx_queue.host.phy);
		log(L_DEBUG, "pRxHostDesc 0x%08X, rxhostdesc_startphy 0x%lX\n",
		    acx2cpu(MemoryConfigOption.pRxHostDesc),
		    (long)adev->hw_rx_queue.host.phy);
		#endif
	}
	else if(IS_MEM(adev)) {
		/*
		 * ACX ignores DMA_config for generic slave mode.
		 */
		#if defined(CONFIG_ACX_MAC80211_MEM)
		MemoryConfigOption.DMA_config = 0;
		/* Declare start of the Rx host pool */
		MemoryConfigOption.pRxHostDesc = cpu2acx(0);
		log(L_DEBUG, "pRxHostDesc 0x%08X, rxhostdesc_startphy 0x%lX\n",
			acx2cpu(MemoryConfigOption.pRxHostDesc),
			(long)adev->hw_rx_queue.host.phy);
		#endif
	}
	else
		MemoryConfigOption.DMA_config = cpu_to_le32(0x20000);

	/* 50% of the allotment of memory blocks go to tx descriptors */
	TxBlockNum = TotalMemoryBlocks / 2;
	MemoryConfigOption.TxBlockNum = cpu_to_le16(TxBlockNum);

	/* and 50% go to the rx descriptors */
	RxBlockNum = TotalMemoryBlocks - TxBlockNum;
	MemoryConfigOption.RxBlockNum = cpu_to_le16(RxBlockNum);

	/* size of the tx and rx descriptor queues */
	TotalTxBlockSize = TxBlockNum * adev->memblocksize;
	TotalRxBlockSize = RxBlockNum * adev->memblocksize;
	log(L_DEBUG, "TxBlockNum %u RxBlockNum %u TotalTxBlockSize %u "
	    "TotalTxBlockSize %u\n", TxBlockNum, RxBlockNum,
	    TotalTxBlockSize, TotalRxBlockSize);

	/* align the tx descriptor queue to an alignment of 0x20 (32 bytes) */
	MemoryConfigOption.rx_mem =
	    cpu_to_le32((le32_to_cpu(mmt->PoolStart) + 0x1f) & ~0x1f);

	/* align the rx descriptor queue to units of 0x20
	 * and offset it by the tx descriptor queue */
	MemoryConfigOption.tx_mem =
	    cpu_to_le32((le32_to_cpu(mmt->PoolStart) + TotalRxBlockSize +
			 0x1f) & ~0x1f);
	log(L_DEBUG, "rx_mem %08X rx_mem %08X\n", MemoryConfigOption.tx_mem,
	    MemoryConfigOption.rx_mem);

	/* alert the device to our decision */
	if (OK !=
	    acx_configure(adev, &MemoryConfigOption,
			    ACX100_IE_MEMORY_CONFIG_OPTIONS))
		goto bad;

	/* and tell the device to kick it into gear */
	if (OK != acx_issue_cmd(adev, ACX100_CMD_INIT_MEMORY, NULL, 0))
		goto bad;

#ifdef CONFIG_ACX_MAC80211_MEM
	/*
	 * slave memory interface has to manage the transmit pools for the ACX,
	 * so it needs to know what we chose here.
	 */
	adev->acx_txbuf_start = MemoryConfigOption.tx_mem;
	adev->acx_txbuf_numblocks = MemoryConfigOption.TxBlockNum;
#endif


	return OK;
bad:

	return NOT_OK;
}

/*
 * acx100_s_create_dma_regions
 *
 * Note that this fn messes up heavily with hardware, but we cannot
 * lock it (we need to sleep). Not a problem since IRQs can't happen
 */
/* OLD CODE? - let's rewrite it! */
static int acx100_create_dma_regions(acx_device_t * adev)
{
	acx100_ie_queueconfig_t queueconf;
	acx_ie_memmap_t memmap;
	int res = NOT_OK;
	u32 tx_queue_start, rx_queue_start;

	adev->num_hw_tx_queues = ACX100_NUM_HW_TX_QUEUES;

	/* read out the acx100 physical start address for the queues */
	if (OK != acx_interrogate(adev, &memmap, ACX1xx_IE_MEMORY_MAP))
		goto fail;

	tx_queue_start = le32_to_cpu(memmap.QueueStart);
	rx_queue_start = tx_queue_start + TX_CNT * sizeof(txdesc_t);

	log(L_DEBUG, "Initializing Queue Indicator\n");

	memset(&queueconf, 0, sizeof(queueconf));

	/* Not needed for PCI, so we can avoid setting them altogether */
	if (IS_USB(adev)) {
		queueconf.NumTxDesc = USB_TX_CNT;
		queueconf.NumRxDesc = USB_RX_CNT;
	}

	/* calculate size of queues */
	queueconf.AreaSize = cpu_to_le32(TX_CNT * sizeof(txdesc_t) +
					 RX_CNT * sizeof(rxdesc_t) + 8);
	queueconf.NumTxQueues = 1;	/* number of tx queues */
	/* sets the beginning of the tx descriptor queue */
	queueconf.TxQueueStart = memmap.QueueStart;
	/* done by memset: queueconf.TxQueuePri = 0; */
	queueconf.RxQueueStart = cpu_to_le32(rx_queue_start);
	queueconf.QueueOptions = 1;	/* auto reset descriptor */
	/* sets the end of the rx descriptor queue */
	queueconf.QueueEnd =
	    cpu_to_le32(rx_queue_start + RX_CNT * sizeof(rxdesc_t)
	    );
	/* sets the beginning of the next queue */
	queueconf.HostQueueEnd =
	    cpu_to_le32(le32_to_cpu(queueconf.QueueEnd) + 8);
	if (OK != acx_configure(adev, &queueconf, ACX100_IE_QUEUE_CONFIG))
		goto fail;

	if (IS_PCI(adev)) {
		/* sets the beginning of the rx descriptor queue,
		 * after the tx descrs */
		if (OK != acx_create_hostdesc_queues(adev, ACX100_NUM_HW_TX_QUEUES))
			goto fail;
		acx_create_desc_queues(adev, rx_queue_start, &tx_queue_start,
		                       ACX100_NUM_HW_TX_QUEUES);
	}
#ifdef CONFIG_ACX_MAC80211_MEM
	else if (IS_MEM(adev)) {
		/* sets the beginning of the rx descriptor queue,
		 * after the tx descrs */
		adev->acx_queue_indicator = (queueindicator_t *)
			(uintptr_t)le32_to_cpu (queueconf.QueueEnd);

		if (OK != acx_create_hostdesc_queues(adev, ACX100_NUM_HW_TX_QUEUES))
			goto fail;

		acx_create_desc_queues(adev, rx_queue_start, &tx_queue_start,
		                       ACX100_NUM_HW_TX_QUEUES);
	}
#endif

	if (OK != acx_interrogate(adev, &memmap, ACX1xx_IE_MEMORY_MAP))
		goto fail;

	memmap.PoolStart = cpu_to_le32((le32_to_cpu(memmap.QueueEnd) + 4 +
					0x1f) & ~0x1f);

	if (OK != acx_configure(adev, &memmap, ACX1xx_IE_MEMORY_MAP))
		goto fail;

	if (OK != acx100_init_memory_pools(adev, &memmap))
		goto fail;

	res = OK;
	goto end;

fail:
	acx_mwait(1000);	/* ? */

	if (IS_PCI(adev) || IS_MEM(adev))
		acx_free_desc_queues(adev);
end:
	return res;
}



int acx_init_mac(acx_device_t * adev)
{
	int result = NOT_OK;

	if (IS_PCI(adev)) {
		adev->memblocksize = 256;	/* 256 is default */
		/* try to load radio for both ACX100 and ACX111, since both
		 * chips have at least some firmware versions making use of an
		 * external radio module */
		acxpci_upload_radio(adev);
	}
	else if (IS_MEM(adev)){
		adev->memblocksize = 256; /* 256 is default */
		/* try to load radio for both ACX100 and ACX111, since both
		 * chips have at least some firmware versions making use of an
		 * external radio module */
		acxmem_upload_radio(adev);
	}
	else
		adev->memblocksize = 128;

	if (IS_ACX111(adev)) {
		/* for ACX111, the order is different from ACX100
		   1. init packet templates
		   2. create station context and create dma regions
		   3. init wep default keys
		 */
		if (OK != acx_init_packet_templates(adev))
			goto fail;
		if (OK != acx111_create_dma_regions(adev)) {
			pr_info("%s: acx111_create_dma_regions FAILED\n",
			       wiphy_name(adev->ieee->wiphy));
			goto fail;
		}

	} else {
		if (OK != acx100_init_wep(adev))
			goto fail;
		if (OK != acx_init_packet_templates(adev))
			goto fail;
		if (OK != acx100_create_dma_regions(adev)) {
			pr_info("%s: acx100_create_dma_regions FAILED\n",
			       wiphy_name(adev->ieee->wiphy));
			goto fail;
		}
	}

	result = OK;
fail:
	if (result)
		pr_info("init_mac() FAILED\n");

	return result;
}

