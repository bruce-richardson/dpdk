/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _ROC_CPT_H_
#define _ROC_CPT_H_

#include "hw/cpt.h"

#include "roc_platform.h"

#define ROC_AE_CPT_BLOCK_TYPE1 0
#define ROC_AE_CPT_BLOCK_TYPE2 1

#define ROC_LOADFVC_MAJOR_OP 0x01UL
#define ROC_LOADFVC_MINOR_OP 0x08UL

/* Default engine groups for CN9K, CN10K */
#define ROC_LEGACY_CPT_DFLT_ENG_GRP_SE	  0UL
#define ROC_LEGACY_CPT_DFLT_ENG_GRP_SE_IE 1UL
#define ROC_LEGACY_CPT_DFLT_ENG_GRP_AE	  2UL

/* Default engine groups */
#define ROC_CPT_DFLT_ENG_GRP_SE 0UL
#define ROC_CPT_DFLT_ENG_GRP_AE 1UL

#define ROC_CPT_MAX_LFS 64
#define ROC_CPT_MAX_BLKS 2
#define ROC_CN10K_CPT_INST_DW_M1                                               \
	((uint64_t)(((sizeof(struct cpt_inst_s) / 16) - 1) & 0x7))
#define ROC_CN10K_TWO_CPT_INST_DW_M1                                           \
	((uint64_t)(((sizeof(struct cpt_inst_s) * 2 / 16) - 1) & 0x7))

/* Vector of sizes in the burst of 16 CPT inst except first in 63:19 of
 * APT_LMT_ARG_S
 */
#define ROC_CN10K_CPT_LMT_ARG                                                                      \
	(ROC_CN10K_CPT_INST_DW_M1 << (19 + 3 * 0) | ROC_CN10K_CPT_INST_DW_M1 << (19 + 3 * 1) |     \
	 ROC_CN10K_CPT_INST_DW_M1 << (19 + 3 * 2) | ROC_CN10K_CPT_INST_DW_M1 << (19 + 3 * 3) |     \
	 ROC_CN10K_CPT_INST_DW_M1 << (19 + 3 * 4) | ROC_CN10K_CPT_INST_DW_M1 << (19 + 3 * 5) |     \
	 ROC_CN10K_CPT_INST_DW_M1 << (19 + 3 * 6) | ROC_CN10K_CPT_INST_DW_M1 << (19 + 3 * 7) |     \
	 ROC_CN10K_CPT_INST_DW_M1 << (19 + 3 * 8) | ROC_CN10K_CPT_INST_DW_M1 << (19 + 3 * 9) |     \
	 ROC_CN10K_CPT_INST_DW_M1 << (19 + 3 * 10) | ROC_CN10K_CPT_INST_DW_M1 << (19 + 3 * 11) |   \
	 ROC_CN10K_CPT_INST_DW_M1 << (19 + 3 * 12) | ROC_CN10K_CPT_INST_DW_M1 << (19 + 3 * 13) |   \
	 ROC_CN10K_CPT_INST_DW_M1 << (19 + 3 * 14))

/* Vector of sizes in the burst of 2 * 16 CPT inst except first in 63:19 of
 * APT_LMT_ARG_S
 */
#define ROC_CN10K_DUAL_CPT_LMT_ARG                                                                 \
	(ROC_CN10K_TWO_CPT_INST_DW_M1 << (19 + 3 * 0) |                                            \
	 ROC_CN10K_TWO_CPT_INST_DW_M1 << (19 + 3 * 1) |                                            \
	 ROC_CN10K_TWO_CPT_INST_DW_M1 << (19 + 3 * 2) |                                            \
	 ROC_CN10K_TWO_CPT_INST_DW_M1 << (19 + 3 * 3) |                                            \
	 ROC_CN10K_TWO_CPT_INST_DW_M1 << (19 + 3 * 4) |                                            \
	 ROC_CN10K_TWO_CPT_INST_DW_M1 << (19 + 3 * 5) |                                            \
	 ROC_CN10K_TWO_CPT_INST_DW_M1 << (19 + 3 * 6) |                                            \
	 ROC_CN10K_TWO_CPT_INST_DW_M1 << (19 + 3 * 7) |                                            \
	 ROC_CN10K_TWO_CPT_INST_DW_M1 << (19 + 3 * 8) |                                            \
	 ROC_CN10K_TWO_CPT_INST_DW_M1 << (19 + 3 * 9) |                                            \
	 ROC_CN10K_TWO_CPT_INST_DW_M1 << (19 + 3 * 10) |                                           \
	 ROC_CN10K_TWO_CPT_INST_DW_M1 << (19 + 3 * 11) |                                           \
	 ROC_CN10K_TWO_CPT_INST_DW_M1 << (19 + 3 * 12) |                                           \
	 ROC_CN10K_TWO_CPT_INST_DW_M1 << (19 + 3 * 13) |                                           \
	 ROC_CN10K_TWO_CPT_INST_DW_M1 << (19 + 3 * 14))

#define ROC_CN20K_CPT_LMT_ARG	     ROC_CN10K_CPT_LMT_ARG
#define ROC_CN20K_DUAL_CPT_LMT_ARG   ROC_CN10K_DUAL_CPT_LMT_ARG
#define ROC_CN20K_CPT_INST_DW_M1     ROC_CN10K_CPT_INST_DW_M1
#define ROC_CN20K_TWO_CPT_INST_DW_M1 ROC_CN10K_TWO_CPT_INST_DW_M1

/* CPT helper macros */
#define ROC_CPT_AH_HDR_LEN	12
#define ROC_CPT_AES_GCM_IV_LEN	8
#define ROC_CPT_AES_GCM_MAC_LEN 16
#define ROC_CPT_AES_CCM_CTR_LEN 4
#define ROC_CPT_AES_CBC_IV_LEN	16
#define ROC_CPT_SHA1_HMAC_LEN	12
#define ROC_CPT_SHA2_HMAC_LEN	16
#define ROC_CPT_DES_IV_LEN	8

#define ROC_CPT_DES3_KEY_LEN	    24
#define ROC_CPT_AES128_KEY_LEN	    16
#define ROC_CPT_AES192_KEY_LEN	    24
#define ROC_CPT_AES256_KEY_LEN	    32
#define ROC_CPT_MD5_KEY_LENGTH	    16
#define ROC_CPT_SHA1_KEY_LENGTH	    20
#define ROC_CPT_SHA256_KEY_LENGTH   32
#define ROC_CPT_SHA384_KEY_LENGTH   48
#define ROC_CPT_SHA512_KEY_LENGTH   64
#define ROC_CPT_AES_XCBC_KEY_LENGTH 16
#define ROC_CPT_AUTH_KEY_LEN_MAX    64

#define ROC_CPT_DES_BLOCK_LENGTH 8
#define ROC_CPT_AES_BLOCK_LENGTH 16

/* Salt length for AES-CTR/GCM/CCM and AES-GMAC */
#define ROC_CPT_SALT_LEN 4

#define ROC_CPT_ESP_HDR_LEN	    8
#define ROC_CPT_ESP_TRL_LEN	    2
#define ROC_CPT_AH_HDR_LEN	    12
#define ROC_CPT_TUNNEL_IPV4_HDR_LEN 20
#define ROC_CPT_TUNNEL_IPV6_HDR_LEN 40

#define ROC_CPT_CCM_AAD_DATA 1
#define ROC_CPT_CCM_MSG_LEN  4
#define ROC_CPT_CCM_ICV_LEN  16
#define ROC_CPT_CCM_FLAGS                                                      \
	((ROC_CPT_CCM_AAD_DATA << 6) |                                         \
	 (((ROC_CPT_CCM_ICV_LEN - 2) / 2) << 3) | (ROC_CPT_CCM_MSG_LEN - 1))
#define ROC_CPT_CCM_SALT_LEN 3

#define ROC_CPT_RES_ALIGN 16

/* Context units in bytes */
#define ROC_CTX_UNIT_8B		  8
#define ROC_CTX_UNIT_128B	  128
#define ROC_CTX_MAX_CKEY_LEN	  32
#define ROC_CTX_MAX_OPAD_IPAD_LEN 128

/* Anti reply window size supported */
#define ROC_AR_WIN_SIZE_MIN	   64
#define ROC_AR_WIN_SIZE_MAX	   4096
#define ROC_LOG_MIN_AR_WIN_SIZE_M1 5

/* u64 array size to fit anti replay window bits */
#define ROC_AR_WINBITS_SZ                                                      \
	(PLT_ALIGN_CEIL(ROC_AR_WIN_SIZE_MAX, BITS_PER_LONG_LONG) /             \
	 BITS_PER_LONG_LONG)

enum {
	ROC_CPT_REVISION_ID_83XX = 0,
	ROC_CPT_REVISION_ID_96XX_B0 = 1,
	ROC_CPT_REVISION_ID_96XX_C0 = 2,
	ROC_CPT_REVISION_ID_98XX = 3,
	ROC_CPT_REVISION_ID_106XX = 4,
};

struct roc_cpt_lmtline {
	uint64_t io_addr;
	uint64_t *fc_addr;
	uintptr_t lmt_base;
	uint32_t fc_thresh;
};

struct roc_cpt_lf {
	/* Input parameters */
	uint16_t lf_id;
	uint32_t nb_desc;
	/* End of Input parameters */
	struct plt_pci_device *pci_dev;
	struct dev *dev;
	struct roc_cpt *roc_cpt;
	uintptr_t rbase;
	uintptr_t lmt_base;
	uint16_t msixoff;
	uint16_t pf_func;
	uint64_t *fc_addr;
	uint64_t io_addr;
	uint8_t *iq_vaddr;
	struct roc_nix *inl_outb_nix;
	uint8_t error_event_pending;
} __plt_cache_aligned;

struct roc_cpt {
	struct plt_pci_device *pci_dev;
	struct roc_cpt_lf *lf[ROC_CPT_MAX_LFS];
	uint16_t nb_lf;
	uint16_t nb_lf_avail;
	uintptr_t lmt_base;
	/**< CPT device capabilities */
	union cpt_eng_caps hw_caps[CPT_MAX_ENG_TYPES];
	uint8_t eng_grp[CPT_MAX_ENG_TYPES];
	uint8_t cpt_revision;
	void *opaque;
#define ROC_CPT_MEM_SZ (6 * 1024)
	uint8_t reserved[ROC_CPT_MEM_SZ] __plt_cache_aligned;
} __plt_cache_aligned;

struct roc_cpt_rxc_time_cfg {
	uint32_t step;
	uint16_t active_limit;
	uint16_t active_thres;
	uint16_t zombie_limit;
	uint16_t zombie_thres;
};

/* CPT MISC interrupt callback */
typedef void (*roc_cpt_int_misc_cb_t)(struct roc_cpt_lf *lf, void *args);

struct roc_cpt_inline_ipsec_inb_cfg {
	uint32_t cpt_credit;
	uint16_t opcode;
	uint16_t param1;
	uint16_t param2;
	uint16_t bpid;
	uint32_t credit_th;
	uint8_t egrp;
	uint8_t ctx_ilen_valid : 1;
	uint8_t ctx_ilen : 7;
};

int __roc_api roc_cpt_rxc_time_cfg(struct roc_cpt *roc_cpt,
				   struct roc_cpt_rxc_time_cfg *cfg);
int __roc_api roc_cpt_dev_init(struct roc_cpt *roc_cpt);
int __roc_api roc_cpt_dev_fini(struct roc_cpt *roc_cpt);
int __roc_api roc_cpt_eng_grp_add(struct roc_cpt *roc_cpt,
				  enum cpt_eng_type eng_type);
int __roc_api roc_cpt_dev_configure(struct roc_cpt *roc_cpt, int nb_lf, bool rxc_ena,
				    uint16_t rx_inject_qp);
void __roc_api roc_cpt_dev_clear(struct roc_cpt *roc_cpt);
int __roc_api roc_cpt_lf_init(struct roc_cpt *roc_cpt, struct roc_cpt_lf *lf);
void __roc_api roc_cpt_lf_fini(struct roc_cpt_lf *lf);
void __roc_api roc_cpt_lf_reset(struct roc_cpt_lf *lf);
int __roc_api roc_cpt_lf_ctx_flush(struct roc_cpt_lf *lf, void *cptr,
				   bool inval);
int __roc_api roc_cpt_lf_ctx_reload(struct roc_cpt_lf *lf, void *cptr);
int __roc_api roc_cpt_inline_ipsec_cfg(struct dev *dev, uint8_t slot,
				       struct roc_nix *nix);
int __roc_api roc_cpt_inline_ipsec_inb_cfg_read(struct roc_cpt *roc_cpt,
					struct roc_cpt_inline_ipsec_inb_cfg *cfg);
int __roc_api roc_cpt_inline_ipsec_inb_cfg(struct roc_cpt *roc_cpt,
					   struct roc_cpt_inline_ipsec_inb_cfg *cfg);
int __roc_api roc_cpt_afs_print(struct roc_cpt *roc_cpt);
int __roc_api roc_cpt_lfs_print(struct roc_cpt *roc_cpt);
void __roc_api roc_cpt_iq_disable(struct roc_cpt_lf *lf);
void __roc_api roc_cpt_iq_enable(struct roc_cpt_lf *lf);
int __roc_api roc_cpt_lmtline_init(struct roc_cpt *roc_cpt, struct roc_cpt_lmtline *lmtline,
				   int lf_id, bool is_dual);

void __roc_api roc_cpt_parse_hdr_dump(FILE *file, const union cpt_parse_hdr_u *cpth);
int __roc_api roc_cpt_ctx_write(struct roc_cpt_lf *lf, void *sa_dptr, void *sa_cptr,
				uint16_t sa_len);

void __roc_api roc_cpt_int_misc_cb_register(roc_cpt_int_misc_cb_t cb, void *args);
int __roc_api roc_cpt_int_misc_cb_unregister(roc_cpt_int_misc_cb_t cb, void *args);

bool roc_cpt_has_ie_engines(void);

#endif /* _ROC_CPT_H_ */
