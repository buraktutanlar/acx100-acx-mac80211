/* Set this to 1 if you want monitor mode to use
** phy header. Currently it is not useful anyway since we
** don't know what useful info (if any) is in phy header.
** If you want faster/smaller code, say 0 here */
#define WANT_PHY_HDR 0

/* whether to do Tx descriptor cleanup in softirq (i.e. not in IRQ
 * handler) or not. Note that doing it later does slightly increase
 * system load, so still do that stuff in the IRQ handler for now,
 * even if that probably means worse latency */
#define TX_CLEANUP_IN_SOFTIRQ 0

/* set to 1 if you want to have 1 driver per card instead of 1 single driver
 * managing all cards (of a particular bus type) in your system
 * Useful e.g. if you need to reinitialize single cards from time to time
 * LINUX 2.4.X ONLY!! (pci_for_each_dev()) Feel free to implement 2.6.x
 * compatibility... */
#define SEPARATE_DRIVER_INSTANCES 0
