struct acx_key {
	u8 enabled:1;
	u8 algorithm;
};

struct acx_stats {        
        u8 link_quality;
        /* Store the last TX/RX times here for updating the leds. */
        unsigned long last_tx;
        unsigned long last_rx;
};
struct acx_interface {                
        /* Opaque ID of the operating interface (!= monitor
         * interface) from the ieee80211 subsystem.
         * Do not modify.
         */
        int if_id;
        /* MAC address. */                  
        u8 *mac_addr;        
        /* Current BSSID (if any). */       
        u8 *bssid;

        /* Interface type. (IEEE80211_IF_TYPE_XXX) */
        int type; 
        /* Counter of active monitor interfaces. */
        int monitor;
        /* Is the card operating in AP, STA or IBSS mode? */
        unsigned int operating:1;           
        /* Promisc mode active?             
         * Note that (monitor != 0) implies promisc.
         */
        unsigned int promisc:1;             
};
#ifdef TODO
# undef TODO
#endif
#define TODO()  \
        do {                                                                            \
                printk(KERN_INFO "TODO: Incomplete code in %s() at %s:%d\n",        \
                       __FUNCTION__, __FILE__, __LINE__);                               \
        } while (0)

#ifdef FIXME
# undef FIXME
#endif
#define FIXME()  \
        do {                                                                            \
                printk(KERN_INFO "FIXME: Possibly broken code in %s() at %s:%d\n",  \
                       __FUNCTION__, __FILE__, __LINE__);                               \
        } while (0)     

#define ACX_MACFMT                "%02x:%02x:%02x:%02x:%02x:%02x"
#define ACX_MACARG(x)       ((u8*)(x))[0], ((u8*)(x))[1], \
                                ((u8*)(x))[2], ((u8*)(x))[3], \
                                ((u8*)(x))[4], ((u8*)(x))[5]


#define ACX_MODE_NOTADHOC 0xFFFF
#define ACX_MODE_PROMISC 0x5

/** Rate values **/
#define ACX_CCK_RATE_1MB            0x02
#define ACX_CCK_RATE_2MB            0x04
#define ACX_CCK_RATE_5MB            0x0B
#define ACX_CCK_RATE_11MB           0x16
#define ACX_OFDM_RATE_6MB           0x0C
#define ACX_OFDM_RATE_9MB           0x12
#define ACX_OFDM_RATE_12MB          0x18
#define ACX_OFDM_RATE_18MB          0x24
#define ACX_OFDM_RATE_24MB          0x30
#define ACX_OFDM_RATE_36MB          0x48
#define ACX_OFDM_RATE_48MB          0x60               
#define ACX_OFDM_RATE_54MB          0x6C
extern struct ethtool_ops acx_ethtool_ops;  
