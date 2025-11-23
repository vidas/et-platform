/*-------------------------------------------------------------------------
* Copyright (c) 2025 Ainekko, Co.
* SPDX-License-Identifier: Apache-2.0
*-------------------------------------------------------------------------*/

// Thread, Hart and Shire ID utilities.

static inline bool shireid_is_ioshire(unsigned shireid)
{
#if EMU_HAS_SVCPROC
    return (shireid == IO_SHIRE_ID);
#else
    (void)shireid;
    return false;
#endif
}

static inline bool shireindex_is_ioshire(unsigned shireidx)
{
#if EMU_HAS_SVCPROC
    return (shireidx == EMU_IO_SHIRE_SP);
#else
    (void)shireidx;
    return false;
#endif
}

static inline unsigned shireid(unsigned shireidx)
{
#if EMU_HAS_SVCPROC
    return shireindex_is_ioshire(shireidx) ? IO_SHIRE_ID : shireidx;
#else
    (void)shireidx;
    return shireidx;
#endif
}

static inline unsigned shireindex(unsigned shireid)
{
#if EMU_HAS_SVCPROC
    return shireid_is_ioshire(shireid) ? EMU_IO_SHIRE_SP : shireid;
#else
    (void)shireid;
    return shireid;
#endif
}

// Minion count per shire.
static inline unsigned shireindex_minions(unsigned shire)
{
    return shireindex_is_ioshire(shire) ? 1 : EMU_MINIONS_PER_SHIRE;
}

// Hart count per shire.
static inline unsigned shireindex_harts(unsigned shire)
{
    return shireindex_is_ioshire(shire) ? 1 : EMU_THREADS_PER_SHIRE;
}

// Neighborhood count per shire.
static inline unsigned shireindex_neighs(unsigned shire)
{
    return shireindex_is_ioshire(shire) ? 1 : EMU_NEIGH_PER_SHIRE;
}

// Harts per minion of a given shire.
static inline unsigned shireindex_minionharts(unsigned shire)
{
    return shireindex_is_ioshire(shire) ? 1 : EMU_THREADS_PER_MINION;
}

static inline bool hartid_is_svcproc(unsigned hartid)
{
#if EMU_HAS_SVCPROC
    return hartid == IO_SHIRE_SP_HARTID;
#else
    (void)hartid;
    return false;
#endif
}

static inline bool hartindex_is_svcproc(unsigned hartindex)
{
#if EMU_HAS_SVCPROC
    return hartindex == EMU_IO_SHIRE_SP_THREAD;
#else
    (void)hartindex;
    return false;
#endif
}

static inline unsigned hartindex(unsigned hartid)
{
#if EMU_HAS_SVCPROC
    return hartid_is_svcproc(hartid) ? EMU_IO_SHIRE_SP_THREAD : hartid;
#else
    return hartid;
#endif
}

static inline unsigned hartid(unsigned hartindex)
{
#if EMU_HAS_SVCPROC
    return hartindex_is_svcproc(hartindex) ? IO_SHIRE_SP_HARTID : hartindex;
#else
    return hartindex;
#endif
}
