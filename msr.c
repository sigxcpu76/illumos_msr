/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright (c) 2004, 2010, Oracle and/or its affiliates. All rights reserved.
 */
/*
 * Copyright (c) 2012, Joyent, Inc.  All rights reserved.
 */


#include <sys/types.h>
#include <sys/file.h>
#include <sys/errno.h>
#include <sys/open.h>
#include <sys/cred.h>
#include <sys/conf.h>
#include <sys/stat.h>
#include <sys/processor.h>
#include <sys/cpuvar.h>
#include <sys/kmem.h>
#include <sys/modctl.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>

#include <sys/auxv.h>
#include <sys/systeminfo.h>

//#if defined(__x86)
#include <sys/x86_archext.h>
//#pragma warn it is fine
//#endif

static dev_info_t *msr_devi;

#define	MSR_DRIVER_NAME	"msr"
#define	MSR_DRIVER_SELF_NODE	"self"

#define	MSR_DIR_NAME		"cpu"
#define	MSR_SELF_DIR_NAME	"self"
#define	MSR_NAME		"msr"
#define	MSR_SELF_NAME		\
		MSR_DIR_NAME "/" MSR_SELF_DIR_NAME "/" MSR_NAME

#define	MSR_SELF_MSR_MINOR	((minor_t)0x3fffful)

/*ARGSUSED*/
static int
msr_getinfo(dev_info_t *devi, ddi_info_cmd_t cmd, void *arg, void **result)
{
	//cmn_err(CE_NOTE, "Inside _getinfo");
	switch (cmd) {
	case DDI_INFO_DEVT2DEVINFO:
	case DDI_INFO_DEVT2INSTANCE:
		break;
	default:
		return (DDI_FAILURE);
	}

	switch (getminor((dev_t)arg)) {
	case MSR_SELF_MSR_MINOR:
		break;
	default:
		return (DDI_FAILURE);
	}

	if (cmd == DDI_INFO_DEVT2INSTANCE)
		*result = 0;
	else
		*result = msr_devi;
	return (DDI_SUCCESS);
}


static int
msr_attach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	//cmn_err(CE_NOTE, "Inside _attach");
	if (cmd != DDI_ATTACH) {
		//cmn_err(CE_NOTE, "Failed to attach");
		return (DDI_FAILURE);
	}
	msr_devi = devi;

	int result = (ddi_create_minor_node(devi, MSR_DRIVER_SELF_NODE, S_IFCHR,
	    MSR_SELF_MSR_MINOR, DDI_PSEUDO, 0));
	//cmn_err(CE_NOTE, "Attach result: %d", result);
	return result;
}

static int
msr_detach(dev_info_t *devi, ddi_detach_cmd_t cmd)
{
	if (cmd != DDI_DETACH)
		return (DDI_FAILURE);
	ddi_remove_minor_node(devi, NULL);
	msr_devi = NULL;
	return (DDI_SUCCESS);
}

/*ARGSUSED1*/
static int
msr_open(dev_t *dev, int flag, int otyp, cred_t *cr)
{
	return (getminor(*dev) == MSR_SELF_MSR_MINOR ? 0 : ENXIO);
}


/*ARGSUSED*/
static int
msr_read(dev_t dev, uio_t *uio, cred_t *cr)
{
	uint64_t msr;
	int error = 0;

	if (!is_x86_feature(x86_featureset, X86FSET_MSR))
		return (ENXIO);

	if (uio->uio_resid & (sizeof (msr) - 1))
		return (EINVAL);

	if(uio->uio_resid < sizeof(msr)) {
		return (EINVAL);
	}

	u_offset_t uoff;
	if ((uoff = (u_offset_t) uio->uio_loffset) > 0xffffffff) {
		return (EINVAL);
	}

	if((error = checked_rdmsr(uoff, &msr)) != 0) {
		return (error);
	}

	if((error = uiomove(&msr, sizeof (msr), UIO_READ, uio)) != 0) {
		return (error);
	}

	return (0);

}


/*ARGSUSED*/

static struct cb_ops msr_cb_ops = {
	msr_open,
	nulldev,	/* close */
	nodev,		/* strategy */
	nodev,		/* print */
	nodev,		/* dump */
	msr_read,
	nodev,		/* write */
	nodev,
	nodev,		/* devmap */
	nodev,		/* mmap */
	nodev,		/* segmap */
	nochpoll,	/* poll */
	ddi_prop_op,
	NULL,
	D_64BIT | D_NEW | D_MP
};

static struct dev_ops msr_dv_ops = {
	DEVO_REV,
	0,
	msr_getinfo,
	nulldev,	/* identify */
	nulldev,	/* probe */
	msr_attach,
	msr_detach,
	nodev,		/* reset */
	&msr_cb_ops,
	(struct bus_ops *)0,
	NULL,
	ddi_quiesce_not_needed,		/* quiesce */
};

static struct modldrv modldrv = {
	&mod_driverops,
	"msr driver",
	&msr_dv_ops
};

static struct modlinkage modl = {
	MODREV_1,
	&modldrv
};

int
_init(void)
{
	return (mod_install(&modl));
}

int
_fini(void)
{
	return (mod_remove(&modl));
}

int
_info(struct modinfo *modinfo)
{
	return (mod_info(&modl, modinfo));
}
