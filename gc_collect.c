#include <inttypes.h>

#include "gc.h"
#include "gc_cheri.h"
#include "gc_collect.h"
#include "gc_debug.h"

void
gc_collect(void)
{
	int rc;

	switch (gc_state_c->gs_mark_state) {
	case GC_MS_MARK:
		gc_resume_marking();
		break;
	case GC_MS_SWEEP:
		gc_resume_sweeping();
		break;
	case GC_MS_NONE:
		gc_debug("beginning a new collection");
#ifdef GC_COLLECT_STATS
		gc_state_c->gs_nmark = 0;
		gc_state_c->gs_nmarkbytes = 0;
		gc_state_c->gs_nsweep = 0;
		gc_state_c->gs_nsweepbytes = 0;
		gc_state_c->gs_ntcollect++;
#endif
		/* Update the VM info. */
		if (gc_vm_tbl_update(&gc_state_c->gs_vt) != GC_SUCC) {
			gc_error("gc_vm_tbl_update");
			return;
		}
		gc_print_vm_tbl(&gc_state_c->gs_vt);
		/* Get the trusted stack. */
		rc = gc_cheri_get_ts(gc_state_c->gs_gts_c);
		if (rc != 0) {
			gc_error("gc_cheri_get_ts error: %d", rc);
			return;
		}
		gc_start_marking();
		/* Because we're not incremental yet: */
		/*while (gc_state_c->mark_state != GC_MS_SWEEP)
			gc_resume_marking();
		while (gc_state_c->mark_state != GC_MS_NONE)
			gc_resume_sweeping();*/
		while (gc_state_c->gs_mark_state != GC_MS_NONE)
			gc_resume_marking();
		/* Restore the trusted stack. */
		rc = gc_cheri_put_ts(gc_state_c->gs_gts_c);
		if (rc != 0) {
			gc_error("gc_cheri_put_ts error: %d", rc);
		return;
	}

		break;
	default:
		/* NOTREACHABLE */
		GC_NOTREACHABLE_ERROR();
		break;
	}
}

int
gc_is_unlimited(_gc_cap void * obj)
{

	/* Rough approximation. */
	return (gc_cheri_gettag(obj) && !gc_cheri_getbase(obj));
}

void
gc_start_marking(void)
{

	gc_state_c->gs_mark_state = GC_MS_MARK;
	gc_push_roots();
	gc_resume_marking();
}

int
gc_push_root(_gc_cap void * _gc_cap *rootp)
{
	int rc;

	if (!gc_cheri_gettag(*rootp) || /* don't push invalid caps */
	    gc_is_unlimited(*rootp)) /* don't push all of memory */
		return (0);
	rc = gc_set_mark(*rootp);
	if (gc_ty_is_free(rc)) {
		/*
		 * Root should not be pointing to this region;
		 * invalidate.
		 */
		*rootp = gc_cheri_cleartag(*rootp);
	} else {
		/* Push whether managed or not. */
		rc = gc_stack_push(gc_state_c->gs_mark_stack_c, *rootp);
		if (rc != 0) {
			gc_error("mark stack overflow");
			return (1);
		}
		if (gc_ty_is_revoked(rc)) {
			/* But also invalidate, so we can scan children. */
			*rootp = gc_cheri_cleartag(*rootp);
		}
	}

	return (0);
}

/* Push roots to mark stack. */
void
gc_push_roots(void)
{
	int i, rc;
	_gc_cap void * _gc_cap *cap;
	size_t ncap;

	gc_debug("push roots:");
	for (i = 0; i < GC_NUM_SAVED_REGS; i++) {
		gc_debug("root: c%d: %s",
		    i >= 10 ? (i >= 12 ? i - 8 : i - 9) : 17 + i,
		    gc_cap_str(gc_state_c->gs_regs_c[i]));
		rc = gc_push_root(&gc_state_c->gs_regs_c[i]);
		if (rc != 0)
			return;
	}

	cap = (_gc_cap void * _gc_cap *)gc_state_c->gs_gts_c;
	ncap = gc_cheri_getlen(gc_state_c->gs_gts_c) /
	    sizeof(_gc_cap void *);
	for (i = 0; i < ncap; i++) {
		gc_debug("root: ts%d: %s", i, gc_cap_str(cap[i]));
		rc = gc_push_root(&cap[i]);
		if (rc != 0)
			return;
	}

	/*
	 * Push a capability to the stack to the mark stack.
	 * This isn't marked as it's not an object that's been allocated
	 * by the collector.
	 */
	gc_debug("root: stack: %s", gc_cap_str(gc_state_c->gs_stack));
	gc_stack_push(gc_state_c->gs_mark_stack_c, gc_state_c->gs_stack);
}


void
gc_scan_tags(_gc_cap void *obj, struct gc_tags tags)
{

	gc_scan_tags_64(obj, tags.tg_lo);
	gc_scan_tags_64(obj + GC_PAGESZ / 2, tags.tg_hi);
}

void
gc_scan_tags_64(_gc_cap void *parent, uint64_t tags)
{
	_gc_cap void * _gc_cap *child_ptr;
	_gc_cap void *obj;
	_gc_cap void *raw_obj;
	_gc_cap struct gc_btbl *bt;
	int rc, error;

	/* Avoid outputting debug messages for zero tags. */
	if (tags == 0)
		return;

	gc_debug("parent: %s, tags 0x%llx", gc_cap_str(parent), tags);
	gc_debug_indent(1);
	for (child_ptr = parent; tags; tags >>= 1, child_ptr++) {
		if (tags & 1) {
			raw_obj = *child_ptr;
			rc = gc_get_obj(gc_unseal(raw_obj), gc_cap_addr(&obj),
			    gc_cap_addr(&bt), NULL, NULL, NULL);
			/* Assert: child has tag bit set! */
			if (gc_ty_is_unmanaged(rc)) {
				/* Mark this object and/or check for already marked. */
				obj = gc_unseal(raw_obj);
				rc = gc_set_mark(obj);
				/* XXX: TODO: if revoked, if free, etc... */
				if (!gc_ty_is_marked(rc)) {
					error = gc_stack_push(
					    gc_state_c->gs_mark_stack_c, raw_obj);
					if (error != 0)
						gc_error("mark stack overflow");
				}
			} else if (gc_ty_is_free(rc)) {
				/* Immediately invalidate (?). */
				*child_ptr = gc_cheri_cleartag(raw_obj);
			} else if (gc_ty_is_used(rc)) {
				/*
				 * gc_set_mark_bt is an optimization;
				 * could call gc_set_mark.
				 */
				if (gc_ty_is_revoked(rc)) {
					/* Immediately invalidate (?), but still push. */
					*child_ptr = gc_cheri_cleartag(raw_obj);
				}
				rc = gc_set_mark_bt(obj, bt);
				/* XXX: assert rc == prev rc? */
				error = gc_stack_push(
				    gc_state_c->gs_mark_stack_c, obj);
				if (error != 0)
					gc_error("mark stack overflow");
			} else if (gc_ty_is_marked(rc)) {
				/* Already marked; ignore. */
			} else {
				/* NOTREACHABLE */
				GC_NOTREACHABLE_ERROR();
			}
		}
	}
	gc_debug_indent(-1);
	gc_debug("== gc_scan_tags_64: finished: %s", gc_cap_str(parent));
}

void
gc_resume_marking(void)
{
	int empty, rc;
	uint8_t type;
	_gc_cap void *obj;
	size_t sml_indx, big_indx;
	_gc_cap struct gc_btbl *btbl;
	_gc_cap struct gc_blk *blk;
	_gc_cap struct gc_vm_ent *ve;

	if (gc_state_c->gs_mark_state == GC_MS_SWEEP) {
		gc_resume_sweeping();
		return;
	}
	empty = gc_stack_pop(gc_state_c->gs_mark_stack_c,
	    gc_cheri_ptr(&obj, sizeof(_gc_cap void*)));
	if (empty) {
#ifdef GC_COLLECT_STATS
		gc_debug("mark phase complete (marked %zu/%zu object(s), "
		    "total %zu/%zu bytes)",
		    gc_state_c->gs_nmark, gc_state_c->gs_nalloc,
		    gc_state_c->gs_nmarkbytes, gc_state_c->gs_nallocbytes);
#endif
		gc_start_sweeping();
		return;
	} else {
		/*
		 * We assume that internal pointers may have been pushed to the
		 * stack, or even unmanaged objects.
		 * So on pop, we first reconstruct the actual object that was
		 * allocated by the collector, to ensure we consider all its
		 * children before marking it (in case future capabilities are a
		 * superset of this capability, and we thus skip it because it's
		 * marked).
		 * If the object wasn't allocated by us, we scan it anyway, but
		 * obviously can't set its mark bit. XXX: Unmanaged cyclic objects
		 * will throw the GC into an infinite loop...
		 */
		gc_debug("popped off the mark stack, raw: %s", gc_cap_str(obj));
		obj = gc_unseal(obj);
		rc = gc_get_obj(obj, gc_cap_addr(&obj),
		    gc_cap_addr(&btbl),
		    gc_cheri_ptr(&big_indx, sizeof(big_indx)),
		    gc_cap_addr(&blk),
		    gc_cheri_ptr(&sml_indx, sizeof(sml_indx)));
		if (gc_ty_is_unmanaged(rc)) {
			gc_debug("warning: unmanaged object: %s", gc_cap_str(obj));
			obj = gc_cheri_setoffset(obj, 0); /* sanitize */
			if (GC_ALIGN(gc_cheri_getbase(obj)) == NULL) {
				gc_debug("warning: popped pointer is near-NULL");
				return;
			}
			btbl = NULL;
			big_indx = 0;
			blk = NULL;
			sml_indx = 0;
			/*
			 * XXX: Object is unmanaged by the GC; use its native
			 * base and length (GROW cap in both directions to
			 * align - correct?).
			 */
			obj = gc_cheri_ptr(GC_ALIGN(gc_cheri_getbase(obj)),
			    GC_ROUND_ALIGN(gc_cheri_getlen(obj)));
			/*
			 * XXX: Need to know if object is "marked" (been scanned
			 * before) to avoid cycles (could implement as perm bit
			 * in scan below; this would bound the number of scans
			 * of the same obj).
			 */

			ve = gc_vm_tbl_find(&gc_state_c->gs_vt,
			    gc_cheri_getbase(obj));
			if (ve == NULL) {
				gc_debug("warning: refusing to scan unmanaged object for which VM info could not be obtained.");
				return;
			}
			/*
			 * Note: we don't do a read/write protection
			 * check here, as individual pages of the object might
			 * might have different permissions (e.g., the
			 * sandbox memory, which contains non-accessible
			 * guard pages.
			 * Instead, gc_mark_children does this check on
			 * a page-by-page basis.
			 */
			gc_debug("VM mapping for unmanaged object: "
			    GC_DEBUG_VE_FMT, GC_DEBUG_VE_PRI(ve));
			/*
			 * As a hacky replacement for gc_get_obj,
			 * look up type in ve->ve_bt.
			 *
			 * XXX: ve_bt always not NULL?
			 */
			btbl = ve->ve_bt;
			rc = gc_get_btbl_indx(btbl, &big_indx, &type, obj);
			if (rc != 0)
				rc = GC_BTBL_UNMANAGED;
			else
				rc = type;
		}
		
		if (gc_ty_is_revoked(rc)) {
			/* Do something? Or handle at the push? */
		} else if (gc_ty_is_free(rc)) {
			/* NOTREACHABLE */
			/* Impossible: a free object is never pushed. */
			gc_error("impossible: gc_get_obj returned GC_OBJ_FREE");
		}
		gc_debug("popped off the mark stack and reconstructed: %s",
		    gc_cap_str(obj));

		gc_mark_children(obj, btbl, big_indx, blk, sml_indx);
	}
}

void
gc_mark_children(_gc_cap void *obj,
    _gc_cap struct gc_btbl *btbl, size_t big_indx,
    _gc_cap struct gc_blk *blk, size_t sml_indx)
{
	size_t page_idx, tag_off, npage, tag_end;
	size_t i, len;
	uintptr_t objlo, objhi, pagelo, pagehi;
	struct gc_tags tags;
	_gc_cap char (*page)[GC_PAGESZ];
	_gc_cap struct gc_vm_ent *ve;
	_gc_cap struct gc_vm_tbl *vt;
	int unmanaged;

	gc_debug("gc_mark_children: scanning object %s\n", gc_cap_str(obj));

	/*
	 * Mark children of object.
	 *
	 * Object might be unmanaged by the GC, in which case btbl
	 * will be NULL.
	 *
	 * For each page spanned by the object, the tags are obtained,
	 * and then that page is scanned by gc_scan_tags. This is
	 * just the outer loop that handles the spanning and tags.
	 *
	 */
	len = gc_cheri_getlen(obj);
	objlo = gc_cheri_getbase(obj);
	objhi = objlo + len;

	pagelo = GC_ALIGN_PAGESZ(objlo);
	pagehi = GC_ROUND_PAGESZ(objhi);
	npage = (pagehi - pagelo) / GC_PAGESZ;
	page = gc_cheri_ptr((void *)pagelo, GC_PAGESZ);
	
	unmanaged = (btbl == NULL) ||
	    !(btbl->bt_flags & GC_BTBL_FLAG_MANAGED);

	if (unmanaged) {
		/*
		 * Unmanaged object. Get the tags directly.
		 */
		vt = &gc_state_c->gs_vt;
		page_idx = 0;

		ve = gc_vm_tbl_find(vt, (uint64_t)(void *)page);
		/* assert(ve != NULL); */ /* guaranteed by caller */
		if (ve->ve_prot & GC_VE_PROT_RD)
			tags = gc_get_page_tags(page);
		else {
			gc_debug("warning: not allowed to read page 0x%llx", (uint64_t)(void *)page);
			tags.tg_lo = 0;
			tags.tg_hi = 0;
		}
	} else {
		page_idx = GC_SLOT_IDX_TO_PAGE_IDX(btbl, big_indx);
		tags = gc_get_or_update_tags(btbl, page_idx);
	}

	tag_off = ((size_t)objlo - pagelo) / GC_TAG_GRAN;
	tag_end = (pagehi - (size_t)objhi) / GC_TAG_GRAN;

	/*
	 * Special case: in the first page, zero out
	 * the bits before the object starts.
	 */
	if (tag_off >= 64) {
		tags.tg_lo = 0;
		tag_off -= 64;
		tags.tg_hi &= ~((1ULL << tag_off) - 1);
	} else {
		tags.tg_lo &= ~((1ULL << tag_off) - 1);
	}

	/* Scan whole pages. */
	for (i = 0; i < npage - 1; i++) {
		gc_scan_tags(page, tags);
		page_idx++;
		page++;
		if (!unmanaged)
			tags = gc_get_or_update_tags(btbl, page_idx);
		else {
			ve = gc_vm_tbl_find(vt, (uint64_t)(void *)page);
			if (ve != NULL && (ve->ve_prot & GC_VE_PROT_RD))
				tags = gc_get_page_tags(page);
			else {
				gc_debug("warning: not allowed to read page 0x%llx", (uint64_t)(void *)page);
				tags.tg_lo = 0;
				tags.tg_hi = 0;
			}
		}
	}

	/*
	 * Special case: in the last page, zero out
	 * the bits after the object ends.
	 */
	if (tag_end >= 64) {
		tags.tg_hi = 0;
		tag_end -= 64;
		tags.tg_lo &= 0xFFFFFFFFFFFFFFFFULL >> tag_end;
	} else {
		tags.tg_hi &= 0xFFFFFFFFFFFFFFFFULL >> tag_end;
	}
	gc_scan_tags(page, tags);
}

void
gc_start_sweeping(void)
{
	_gc_cap void *ptr;
	int error;

	gc_debug("begin sweeping");
	gc_state_c->gs_mark_state = GC_MS_SWEEP;

	/* Push the btbls to consider on to the sweep stack. */
	ptr = &gc_state_c->gs_btbl_small;
	ptr = gc_cheri_incbase(ptr, gc_cheri_getoffset(ptr));
	ptr = gc_cheri_setoffset(ptr, 0);
	ptr = gc_cheri_setlen(ptr, sizeof(struct gc_btbl));
	error = gc_stack_push(gc_state_c->gs_sweep_stack_c, ptr);
	if (error != 0) {
		gc_error("sweep stack overflow");
		return;
	}
	ptr = &gc_state_c->gs_btbl_big;
	ptr = gc_cheri_incbase(ptr, gc_cheri_getoffset(ptr));
	ptr = gc_cheri_setoffset(ptr, 0);
	ptr = gc_cheri_setlen(ptr, sizeof(struct gc_btbl));
	error = gc_stack_push(gc_state_c->gs_sweep_stack_c, ptr);
	if (error != 0) {
		gc_error("sweep stack overflow");
		return;
	}

	gc_resume_sweeping();
}

void
gc_resume_sweeping(void)
{
	_gc_cap struct gc_btbl *btbl;
	void *addr;
	int empty, i, j, small, freecont;
	uint8_t byte, type;
	size_t npages;

	empty = gc_stack_pop(gc_state_c->gs_sweep_stack_c, gc_cap_addr(&btbl));
	if (empty) {
		/* Collection complete. */
#ifdef GC_COLLECT_STATS
		gc_debug("sweep phase complete (swept %zu/%zu object(s), "
		    "total recovered %zu/%zu bytes)",
		    gc_state_c->gs_nsweep, gc_state_c->gs_nalloc,
		    gc_state_c->gs_nsweepbytes, gc_state_c->gs_nallocbytes);
#endif
		gc_state_c->gs_mark_state = GC_MS_NONE;
#ifdef GC_COLLECT_STATS
		gc_state_c->gs_nalloc -= gc_state_c->gs_nsweep;
		gc_state_c->gs_nallocbytes -= gc_state_c->gs_nsweepbytes;
#endif
		return;
	}
	small = btbl->bt_flags & GC_BTBL_FLAG_SMALL;
	/* Walk the btbl, making objects and entire blocks free. */
	freecont = 0;
	for (i = 0; i < btbl->bt_nslots / 2; i++) {
		byte = btbl->bt_map[i];
		for (j = 0; j < 2; j++) {
			type = GC_BTBL_GETTYPE(byte, j);
			addr = (char*)gc_cheri_getbase(btbl->bt_base) +
					GC_BTBL_MKINDX(i, j) * btbl->bt_slotsz;
			if (!small)
				gc_sweep_large_iter(btbl, &byte, type, addr, j,
				    &freecont);
			else
				gc_sweep_small_iter(btbl, &byte, type, addr, j);
		}
		btbl->bt_map[i] = byte;
	}

	/*
	 * Invalidate knowledge of tag bits for all pages stored in
	 * this block table.
	 *
	 * Currently slow, and doesn't take into account page
	 * protection bits: the tag bits of a non-writable page
	 * will not change until it is made writable.
	 *
	 */
	npages = (btbl->bt_slotsz * btbl->bt_nslots) / GC_PAGESZ;
	for (i = 0; i < npages; i++)
		btbl->bt_tags[i].tg_v = 0;
}

void
gc_sweep_large_iter(_gc_cap struct gc_btbl *btbl, uint8_t *byte,
    uint8_t type, void *addr, int j, int *freecont)
{
	_gc_cap void *p;

	p = gc_cheri_ptr(addr, btbl->bt_slotsz);

	if (type == GC_BTBL_CONT && *freecont) {
		/*
		 * Freeing continuation data.
		 */
		GC_BTBL_SETTYPE(*byte, j, GC_BTBL_FREE);
		gc_fill_free_mem(p);
#ifdef GC_COLLECT_STATS
		gc_state_c->gs_nsweepbytes += btbl->bt_slotsz;
#endif
	} else if (type == GC_BTBL_USED) {
		/* Used but not marked; free entire block. */
		GC_BTBL_SETTYPE(*byte, j, GC_BTBL_FREE);
		gc_fill_free_mem(p);
		/* Next iterations will free continuation data. */
		*freecont = 1;
#ifdef GC_COLLECT_STATS
		gc_state_c->gs_nsweep++;
		gc_state_c->gs_nsweepbytes += btbl->bt_slotsz;
#endif
		/*gc_debug("swept entire large "
		    "block at address %p",
		    addr);*/
	} else if (type == GC_BTBL_USED_MARKED) {
		/*
		 * Used and marked; keep block and following
		 * continuation data.
		 */
		GC_BTBL_SETTYPE(*byte, j, GC_BTBL_USED);
		*freecont = 0;
	} else if (*freecont) {
		*freecont = 0;
	}
}

void
gc_sweep_small_iter(_gc_cap struct gc_btbl *btbl, uint8_t *byte,
    uint8_t type, void *addr, int j)
{
	_gc_cap struct gc_blk *blk;
	int hdrbits;
	uint64_t tmp;
	int k;

	blk = gc_cheri_ptr(addr, btbl->bt_slotsz);
	if (type == GC_BTBL_USED) {
		hdrbits = (GC_BLK_HDRSZ + blk->bk_objsz - 1) / blk->bk_objsz;
#ifdef GC_COLLECT_STATS
		/*
		 *  free  mark  NOR  meaning
		 *  0     0     1    swept (count)
		 *  0     1     0    alive (don't count)
		 *  1     0     0    don't care
		 *  1     1     0    "impossible"
		 */
		tmp = ~(blk->bk_free | blk->bk_marks);
		tmp &= ((1ULL << (GC_PAGESZ / blk->bk_objsz)) - 1ULL);
		tmp &= ~((1ULL << hdrbits) - 1ULL);
		for (k = 0; tmp; tmp >>= 1, k++) {
			if (tmp & 1) {
				gc_state_c->gs_nsweep++;
				gc_state_c->gs_nsweepbytes += blk->bk_objsz;
				// TODO: implement gc_get_blk_obj gc_fill_free_mem(gc_get_blk_obj(blk, k));
			}
		}
#endif
		if (!blk->bk_marks) {
			/* Entire block free. Remove it from its list. */
			GC_BTBL_SETTYPE(*byte, j, GC_BTBL_FREE);
			if (blk->bk_next)
				blk->bk_next->bk_prev = blk->bk_prev;
			if (blk->bk_prev)
				blk->bk_prev->bk_next = blk->bk_next;
			gc_debug("swept entire block "
			    "storing objects of size "
			    "%zu at address %s",
			    blk->bk_objsz,
			    gc_cap_str(blk));
			gc_fill_free_mem(blk);
		} else {
			/* Make free all those things that aren't marked. */
			blk->bk_free = ~blk->bk_marks;
			blk->bk_free &= ((1ULL << (GC_PAGESZ / blk->bk_objsz)) - 1ULL);
			/* Account for the space taken up by the block header. */
			blk->bk_free &= ~((1ULL << hdrbits) - 1ULL);
			blk->bk_marks = 0;
			gc_debug("swept some objects "
			    "of size %zu in block %s",
			    blk->bk_objsz,
			    gc_cap_str(blk));
		}
	}
}
