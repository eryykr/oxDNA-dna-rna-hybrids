/*
 * NathanInteraction.h
 *
 *  Created on: 21 Aug 2014
 *      Author: lorenzo
 */

#ifndef NATHANINTERACTION_H_
#define NATHANINTERACTION_H_

#include "Interactions/BaseInteraction.h"

class NathanInteraction: public BaseInteraction<NathanInteraction> {
protected:
	/// Repulsive interaction energy at the cut-off
	number _rep_E_cut;
	int _rep_power;

	/// Cosine of the patch half width
	number _patch_cosmax;
	number _patch_pow_sigma;
	int _patch_power;
	/// Cut-off for the attractive part. It is computed at runtime
	number _patch_cutoff;
	/// Patch-patch interaction energy at the cut-off
	number _patch_E_cut;
	/// Width of the patch, defaults to 0.12
	number _patch_alpha;
	/// _patch_alpha^10
	number _patch_pow_alpha;

	int _N_patchy;
	int _N_polymers;
	int _N_per_chain;
	int _N_chains;

	number _rfene, _sqr_rfene;
	number _pol_rcut, _sqr_pol_rcut;
	number _pol_sigma, _sqr_pol_sigma;
	number _pol_patchy_sigma, _sqr_pol_patchy_sigma;
	int _pol_n;

	inline number _fene(BaseParticle *p, BaseParticle *q, bool compute_r, bool update_forces);
	inline number _nonbonded(BaseParticle *p, BaseParticle *q, bool compute_r, bool update_forces);
	inline number _patchy_interaction(BaseParticle *p, BaseParticle *q, bool compute_r, bool update_forces);
public:
	enum {
		PATCHY_PATCHY = 0, PATCHY_POLYMER = 1, POLYMER_POLYMER = 2
	};

	enum {
		PATCHY_PARTICLE = 0, POLYMER = 1
	};

	NathanInteraction();
	virtual ~NathanInteraction();

	virtual void get_settings(input_file &inp);
	virtual void init();

	virtual void generate_random_configuration(std::vector<BaseParticle *> &particles);

	virtual void allocate_particles(std::vector<BaseParticle *> &particles);
	virtual int get_N_from_topology();
	virtual void read_topology(int *N_strands, std::vector<BaseParticle *> &particles);

	virtual number pair_interaction(BaseParticle *p, BaseParticle *q, bool compute_r = true, bool update_forces = false);
	virtual number pair_interaction_bonded(BaseParticle *p, BaseParticle *q, bool compute_r = true, bool update_forces = false);
	virtual number pair_interaction_nonbonded(BaseParticle *p, BaseParticle *q, bool compute_r = true, bool update_forces = false);
	virtual number pair_interaction_term(int name, BaseParticle *p, BaseParticle *q, bool compute_r = true, bool update_forces = false) {
		return this->_pair_interaction_term_wrapper(this, name, p, q, compute_r, update_forces);
	}

	virtual void check_input_sanity(std::vector<BaseParticle *> &particles);
};

number NathanInteraction::_patchy_interaction(BaseParticle *p, BaseParticle *q, bool compute_r, bool update_forces) {
	if(p->type != PATCHY_PARTICLE && q->type != PATCHY_PARTICLE) return 0.f;
	number rnorm = r->norm();
	if(rnorm > this->_sqr_rcut) return (number) 0.f;

	number energy = (number) 0.f;

	// repulsion
	number part = 1. / pow(rnorm, _rep_power * 0.5f);
	energy = part - _rep_E_cut;

	if(update_forces) {
		LR_vector force = *r * (_rep_power * part / rnorm);
		p->force -= force;
		q->force += force;
	}

	// attraction: here everything is done as in Allen's paper
	number rmod = sqrt(rnorm);
	LR_vector p_axis = p->orientationT.v3;
	LR_vector q_axis = q->orientationT.v3;
	LR_vector r_versor = *r / (-rmod);

	number cospr = -(p_axis * r_versor);
	if(cospr < 0.) {
		p_axis = -p_axis;
		cospr = -cospr;
	}
	number cosqr = q_axis * r_versor;
	if(cosqr < 0.) {
		q_axis = -q_axis;
		cosqr = -cosqr;
	}
	number p_mod = exp(-pow(cospr - 1., _patch_power) / (2. * _patch_pow_sigma));
	number q_mod = exp(-pow(cosqr - 1., _patch_power) / (2. * _patch_pow_sigma));
	if(p_mod < 1e-6 || q_mod < 1e-6) return energy;

	number sqr_surf_dist = SQR(rmod - 1.);
	number r8b10 = SQR(SQR(sqr_surf_dist)) / _patch_pow_alpha;
	number exp_part = -1.001 * exp(-(number) 0.5 * r8b10 * sqr_surf_dist);
//	energy += (exp_part - _patch_E_cut)*p_mod*q_mod;
	energy += exp_part * p_mod * q_mod;

	if(update_forces) {
		// radial part
		LR_vector tmp_force = r_versor * (p_mod * q_mod * 5. * (rmod - 1.) * exp_part * r8b10);

		// angular p part
		number der_p = exp_part * q_mod * (0.5 * _patch_power * p_mod * pow(cospr - 1., _patch_power - 1.) / _patch_pow_sigma);
		LR_vector p_ortho = p_axis + cospr * r_versor;
		tmp_force -= p_ortho * (der_p / rmod);

		// angular q part
		number der_q = exp_part * p_mod * (-0.5 * _patch_power * q_mod * pow(cosqr - 1., _patch_power - 1.) / _patch_pow_sigma);
		LR_vector q_ortho = q_axis - cosqr * r_versor;
		tmp_force -= q_ortho * (der_q / rmod);

		p->force += tmp_force;
		q->force -= tmp_force;

		p->torque += p->orientationT * (r_versor.cross(p_axis) * der_p);
		q->torque += q->orientationT * (r_versor.cross(q_axis) * der_q);
	}

	return energy;
}

class NathanPatchyParticle: public BaseParticle {
public:
	NathanPatchyParticle() :
					BaseParticle() {
	}
	;
	virtual ~NathanPatchyParticle() {
	}
	;

	virtual bool is_rigid_body() {
		return true;
	}
};

class NathanPolymerParticle: public BaseParticle {
public:
	NathanPolymerParticle() :
					BaseParticle() {
	}
	;
	virtual ~NathanPolymerParticle() {
	}
	;

	virtual bool is_rigid_body() {
		return false;
	}
	virtual bool is_bonded(BaseParticle *q) {
		if(q->type == NathanInteraction::POLYMER) {
			return (q == this->n3 || q == this->n5);
		}
		return false;
	}
};

extern "C" NathanInteraction *make_NathanInteraction() {
	return new NathanInteraction();
}

#endif /* NATHANINTERACTION_H_ */
