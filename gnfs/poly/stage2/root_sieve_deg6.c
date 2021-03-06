/*--------------------------------------------------------------------
This source distribution is placed in the public domain by its author,
Jason Papadopoulos. You may use it for any purpose, free of charge,
without having to notify anyone. I disclaim any responsibility for any
errors.

Optionally, please be nice and tell me if you find this source to be
useful. Again optionally, if you add to the functionality present here
please consider making those additions public too, so that others may 
benefit from your work.	

$Id$
--------------------------------------------------------------------*/

#include "stage2.h"

/*-------------------------------------------------------------------------*/
#define APPLY_ROTATION(dist) {						\
	double x_dist = floor((dist) * direction[0]);			\
	double y_dist = floor((dist) * direction[1]);			\
	double z_dist = floor((dist) * direction[2]);			\
	bpoly.coeff[0] = apoly->coeff[0] - dbl_d * x_dist;		\
	bpoly.coeff[1] = apoly->coeff[1] + dbl_p * x_dist - dbl_d * y_dist; \
	bpoly.coeff[2] = apoly->coeff[2] + dbl_p * y_dist - dbl_d * z_dist; \
	bpoly.coeff[3] = apoly->coeff[3] + dbl_p * z_dist;		\
}

void
compute_line_size_deg6(double max_norm, dpoly_t *apoly,
		  double dbl_p, double dbl_d, double direction[3],
		  double last_line_min_in, double last_line_max_in,
		  double *line_min, double *line_max)
{
	double v0;
	double new_xlate, new_skewness;
	double d0, d1, offset;
	double last_line_min = last_line_min_in;
	double last_line_max = last_line_max_in;
	dpoly_t bpoly = *apoly;

	APPLY_ROTATION(last_line_min);
	v0 = optimize_basic(&bpoly, &new_skewness, &new_xlate);
	offset = 1e-6 * fabs(last_line_min);
	offset = MAX(offset, 10000.0);

	if (v0 > max_norm) {
		d0 = last_line_min;
		d1 = last_line_min + offset;
		while (1) {
			double new_d1 = last_line_min + offset;
			APPLY_ROTATION(new_d1);
			v0 = optimize_basic(&bpoly, &new_skewness, &new_xlate);
			if (v0 <= max_norm || new_d1 >= last_line_max) {
				d1 = new_d1;
				break;
			}
			d0 = new_d1;
			offset *= 2;
		}
	}
	else {
		d0 = last_line_min - offset;
		d1 = last_line_min;
		while (1) {
			double new_d0 = last_line_min - offset;
			APPLY_ROTATION(new_d0);
			v0 = optimize_basic(&bpoly, &new_skewness, &new_xlate);
			if (v0 > max_norm) {
				d0 = new_d0;
				break;
			}
			d1 = new_d0;
			offset *= 2;
		}
	}

	while (d1 - d0 > 500 && d1 - d0 > 1e-6 * fabs(d0)) {
		double dd = (d0 + d1) / 2;
		APPLY_ROTATION(dd);
		v0 = optimize_basic(&bpoly, &new_skewness, &new_xlate);
		if (v0 > max_norm)
			d0 = dd;
		else
			d1 = dd;
	}
	*line_min = d0;

	APPLY_ROTATION(last_line_max);
	v0 = optimize_basic(&bpoly, &new_skewness, &new_xlate);
	offset = 1e-6 * fabs(last_line_max);
	offset = MAX(offset, 10000.0);

	if (v0 > max_norm) {
		d0 = last_line_max - offset;
		d1 = last_line_max;
		while (1) {
			double new_d0 = last_line_max - offset;
			APPLY_ROTATION(new_d0);
			v0 = optimize_basic(&bpoly, &new_skewness, &new_xlate);
			if (v0 <= max_norm || new_d0 <= last_line_min) {
				d0 = new_d0;
				break;
			}
			d1 = new_d0;
			offset *= 2;
		}
	}
	else {
		d0 = last_line_max;
		d1 = last_line_max + offset;
		while (1) {
			double new_d1 = last_line_max + offset;
			APPLY_ROTATION(new_d1);
			v0 = optimize_basic(&bpoly, &new_skewness, &new_xlate);
			if (v0 > max_norm) {
				d1 = new_d1;
				break;
			}
			d0 = new_d1;
			offset *= 2;
		}
	}

	while (d1 - d0 > 500 && d1 - d0 > 1e-6 * fabs(d0)) {
		double dd = (d0 + d1) / 2;
		APPLY_ROTATION(dd);
		v0 = optimize_basic(&bpoly, &new_skewness, &new_xlate);
		if (v0 > max_norm)
			d1 = dd;
		else
			d0 = dd;
	}
	*line_max = d1;
}

/*-------------------------------------------------------------------------*/
#define CRT_ACCUM(idx)						\
	crt_score[idx] = crt_score[(idx)+1] +                   \
				hitlist[idx].score[i##idx];	\
	for (j = 0; j < dim; j++) {				\
		crt_accum[idx][j] = crt_accum[(idx)+1][j] +	\
				crt_prod[idx] * 		\
				hitlist[idx].roots[i##idx][j];	\
	}

void
compute_lattices(hit_t *hitlist, uint32 num_lattice_primes,
		   lattice_t *lattices, uint64 lattice_size,
		   uint32 num_lattices, uint32 dim)
{
	uint32 i, j;
	int32 i0, i1, i2, i3, i4, i5, i6, i7, i8, i9;
	uint64 crt_prod[MAX_CRT_FACTORS];
	uint64 crt_accum[MAX_CRT_FACTORS + 1][3];
	uint16 crt_score[MAX_CRT_FACTORS + 1];

	for (i = 0; i < num_lattice_primes; i++) {
		hit_t *hits = hitlist + i;
		uint32 p = hits->power;

		crt_prod[i] = lattice_size / p;
		crt_prod[i] *= mp_modinv_1((uint32)(crt_prod[i] % p), p);
	}

	i0 = i1 = i2 = i3 = i4 = i5 = i6 = i7 = i8 = i9 = i = 0;
	crt_score[num_lattice_primes] = 0;
	crt_accum[num_lattice_primes][0] = 0;
	crt_accum[num_lattice_primes][1] = 0;
	crt_accum[num_lattice_primes][2] = 0;

	switch (num_lattice_primes) {
	case 10:
		for (i9 = hitlist[9].num_roots - 1; i9 >= 0; i9--) {
			CRT_ACCUM(9)
	case 9:
		for (i8 = hitlist[8].num_roots - 1; i8 >= 0; i8--) {
			CRT_ACCUM(8)
	case 8:
		for (i7 = hitlist[7].num_roots - 1; i7 >= 0; i7--) {
			CRT_ACCUM(7)
	case 7:
		for (i6 = hitlist[6].num_roots - 1; i6 >= 0; i6--) {
			CRT_ACCUM(6)
	case 6:
		for (i5 = hitlist[5].num_roots - 1; i5 >= 0; i5--) {
			CRT_ACCUM(5)
	case 5:
		for (i4 = hitlist[4].num_roots - 1; i4 >= 0; i4--) {
			CRT_ACCUM(4)
	case 4:
		for (i3 = hitlist[3].num_roots - 1; i3 >= 0; i3--) {
			CRT_ACCUM(3)
	case 3:
		for (i2 = hitlist[2].num_roots - 1; i2 >= 0; i2--) {
			CRT_ACCUM(2)
	case 2:
		for (i1 = hitlist[1].num_roots - 1; i1 >= 0; i1--) {
			CRT_ACCUM(1)
	default:
		for (i0 = hitlist[0].num_roots - 1; i0 >= 0; i0--) {
			CRT_ACCUM(0)

			if (i == num_lattices)
				return;

			lattices[i].score = crt_score[0];
			lattices[i].x = crt_accum[0][0] % lattice_size;
			if (dim > 1)
				lattices[i].y = crt_accum[0][1] % lattice_size;
			if (dim > 2)
				lattices[i].z = crt_accum[0][2] % lattice_size;
			i++;
		}}}}}}}}}}
	}
}

/*-------------------------------------------------------------------------*/
/* boilerplate code for managing heaps */

#define HEAP_SWAP(a,b) { tmp = a; a = b; b = tmp; }
#define HEAP_PARENT(i)  (((i)-1) >> 1)
#define HEAP_LEFT(i)    (2 * (i) + 1)
#define HEAP_RIGHT(i)   (2 * (i) + 2)

static void
heapify(mp_rotation_t *h, uint32 index, uint32 size) {

	uint32 c;
	mp_rotation_t tmp;
	for (c = HEAP_LEFT(index); c < (size-1); 
			index = c, c = HEAP_LEFT(index)) {

		if (h[c].score < h[c+1].score)
			c++;

		if (h[index].score < h[c].score) {
			HEAP_SWAP(h[index], h[c]);
		}
		else {
			return;
		}
	}
	if (c == (size-1) && h[index].score < h[c].score) {
		HEAP_SWAP(h[index], h[c]);
	}
}

static void
make_heap(mp_rotation_t *h, uint32 size) {

	int32 i;
	for (i = HEAP_PARENT(size); i >= 0; i--)
		heapify(h, (uint32)i, size);
}

void
save_mp_rotation(root_heap_t *heap, mpz_t x, mpz_t y,
		int64 z, float score)
{
	mp_rotation_t *h = heap->mp_entries;

	if (heap->num_entries <= heap->max_entries - 1) {
		mp_rotation_t *r = h + heap->num_entries++;
		mpz_set(r->x, x);
		mpz_set(r->y, y);
		r->z = z;
		r->score = score;
		if (heap->num_entries == heap->max_entries)
			make_heap(h, heap->max_entries);
	}
	else if (h->score > score) {
		mpz_set(h->x, x);
		mpz_set(h->y, y);
		h->z = z;
		h->score = score;
		heapify(h, 0, heap->max_entries);
	}
}

/*-------------------------------------------------------------------------*/
static const double size_bound[] = {1.05, 1.5, 4.0};
#define NUM_BOUNDS (sizeof(size_bound) / sizeof(size_bound[0]))

void
root_sieve_run_deg6(poly_stage2_t *data, double curr_norm,
			double alpha_proj)
{
	uint32 i;
	stage2_curr_data_t *s = (stage2_curr_data_t *)data->internal;
	root_sieve_t *rs = &s->root_sieve;
	curr_poly_t *c = &s->curr_poly;

	rs->root_heap.extra = data; /* FIXME */
	rs->root_heap.num_entries = 0;
	rs->dbl_p = mpz_get_d(c->gmp_p);
	rs->dbl_d = mpz_get_d(c->gmp_d);
	rs->apoly.degree = data->degree;
	for (i = 0; i <= data->degree; i++)
		rs->apoly.coeff[i] = mpz_get_d(c->gmp_a[i]);

	for (i = 0; i < NUM_BOUNDS; i++) {
		double bound = MIN(size_bound[i] * curr_norm,
						data->max_norm);

		rs->max_norm = exp(-alpha_proj) * bound;

		sieve_xyz_run(rs);

		if (bound == data->max_norm)
			break;
	}

	for (i = 0; i < rs->root_heap.num_entries; i++) {
		mp_rotation_t *r = rs->root_heap.mp_entries + i;

		optimize_final(r->x, r->y, r->z, data);
	}
}
