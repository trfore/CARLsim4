/* * Copyright (c) 2015 Regents of the University of California. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. The names of its contributors may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * *********************************************************************************************** *
 * CARLsim
 * created by: (MDR) Micah Richert, (JN) Jayram M. Nageswaran
 * maintained by:
 * (MA) Mike Avery <averym@uci.edu>
 * (MB) Michael Beyeler <mbeyeler@uci.edu>,
 * (KDC) Kristofor Carlson <kdcarlso@uci.edu>
 * (TSC) Ting-Shuo Chou <tingshuc@uci.edu>
 *
 * CARLsim available from http://socsci.uci.edu/~jkrichma/CARLsim/
 * Ver 5/22/2015
 */

#include <snn.h>

// This method loops through all spikes that are generated by neurons with a delay of 1ms
// and delivers the spikes to the appropriate post-synaptic neuron
void SNN::doD1CurrentUpdate() {
	int k     = spikeCountD1Sec-1;
	int k_end = timeTableD1[simTimeMs+maxDelay_];

	while((k>=k_end) && (k>=0)) {

		int neuron_id = firingTableD1[k];
		assert(neuron_id<numN);

		delay_info_t dPar = snnRuntimeData.postDelayInfo[neuron_id*(maxDelay_+1)];

		unsigned int  offset = snnRuntimeData.cumulativePost[neuron_id];

		for(int idx_d = dPar.delay_index_start;
			idx_d < (dPar.delay_index_start + dPar.delay_length);
			idx_d = idx_d+1) {
				generatePostSpike( neuron_id, idx_d, offset, 0);
		}
		k=k-1;
	}
}

// This method loops through all spikes that are generated by neurons with a delay of 2+ms
// and delivers the spikes to the appropriate post-synaptic neuron
void SNN::doD2CurrentUpdate() {
	int k = spikeCountD2Sec-1;
	int k_end = timeTableD2[simTimeMs+1];
	int t_pos = simTimeMs;

	while((k>=k_end)&& (k >=0)) {

		// get the neuron id from the index k
		int i  = firingTableD2[k];

		// find the time of firing from the timeTable using index k
		while (!((k >= timeTableD2[t_pos+maxDelay_])&&(k < timeTableD2[t_pos+maxDelay_+1]))) {
			t_pos = t_pos - 1;
			assert((t_pos+maxDelay_-1)>=0);
		}

		// \TODO: Instead of using the complex timeTable, can neuronFiringTime value...???
		// Calculate the time difference between time of firing of neuron and the current time...
		int tD = simTimeMs - t_pos;

		assert((tD<maxDelay_)&&(tD>=0));
		assert(i<numN);

		delay_info_t dPar = snnRuntimeData.postDelayInfo[i*(maxDelay_+1)+tD];

		unsigned int offset = snnRuntimeData.cumulativePost[i];

		// for each delay variables
		for(int idx_d = dPar.delay_index_start;
			idx_d < (dPar.delay_index_start + dPar.delay_length);
			idx_d = idx_d+1) {
			generatePostSpike( i, idx_d, offset, tD);
		}

		k=k-1;
	}
}

void SNN::doSnnSim() {
	// for all Spike Counters, reset their spike counts to zero if simTime % recordDur == 0
	if (sim_with_spikecounters) {
		checkSpikeCounterRecordDur();
	}

	// decay STP vars and conductances
	doSTPUpdateAndDecayCond();

	updateSpikeGenerators();

	//generate all the scheduled spikes from the spikeBuffer..
	generateSpikes();

	// find the neurons that has fired..
	findFiring();

	timeTableD2[simTimeMs+maxDelay_+1] = spikeCountD2Sec;
	timeTableD1[simTimeMs+maxDelay_+1] = spikeCountD1Sec;

	doD2CurrentUpdate();
	doD1CurrentUpdate();
	globalStateUpdate();

	return;
}

void SNN::doSTPUpdateAndDecayCond() {
	int spikeBufferFull = 0;

	//decay the STP variables before adding new spikes.
	for(int g=0; (g < numGroups) & !spikeBufferFull; g++) {
		for(int i=groupConfig[g].StartN; i<=groupConfig[g].EndN; i++) {
	   		//decay the STP variables before adding new spikes.
			if (groupConfig[g].WithSTP) {
				int ind_plus  = STP_BUF_POS(i,simTime);
				int ind_minus = STP_BUF_POS(i,(simTime-1));
				snnRuntimeData.stpu[ind_plus] = snnRuntimeData.stpu[ind_minus]*(1.0-groupConfig[g].STP_tau_u_inv);
				snnRuntimeData.stpx[ind_plus] = snnRuntimeData.stpx[ind_minus] + (1.0-snnRuntimeData.stpx[ind_minus])*groupConfig[g].STP_tau_x_inv;
			}

			if (groupConfig[g].Type&POISSON_NEURON)
				continue;

			// decay conductances
			if (sim_with_conductances) {
				snnRuntimeData.gAMPA[i]  *= dAMPA;
				snnRuntimeData.gGABAa[i] *= dGABAa;

				if (sim_with_NMDA_rise) {
					snnRuntimeData.gNMDA_r[i] *= rNMDA;	// rise
					snnRuntimeData.gNMDA_d[i] *= dNMDA;	// decay
				} else {
					snnRuntimeData.gNMDA[i]   *= dNMDA;	// instantaneous rise
				}

				if (sim_with_GABAb_rise) {
					snnRuntimeData.gGABAb_r[i] *= rGABAb;	// rise
					snnRuntimeData.gGABAb_d[i] *= dGABAb;	// decay
				} else {
					snnRuntimeData.gGABAb[i] *= dGABAb;	// instantaneous rise
				}
			}
			else {
				snnRuntimeData.current[i] = 0.0f; // in CUBA mode, reset current to 0 at each time step and sum up all wts
			}
		}
	}
}

void SNN::findFiring() {
	int spikeBufferFull = 0;

	for(int g=0; (g < numGroups) & !spikeBufferFull; g++) {
		// given group of neurons belong to the poisson group....
		if (groupConfig[g].Type&POISSON_NEURON)
			continue;

		// his flag is set if with_stdp is set and also grpType is set to have GROUP_SYN_FIXED
		for(int i=groupConfig[g].StartN; i <= groupConfig[g].EndN; i++) {

			assert(i < numNReg);

			if (snnRuntimeData.voltage[i] >= 30.0) {
				snnRuntimeData.voltage[i] = snnRuntimeData.Izh_c[i];
				snnRuntimeData.recovery[i] += snnRuntimeData.Izh_d[i];

				// if flag hasSpkMonRT is set, we want to keep track of how many spikes per neuron in the group
				if (groupConfig[g].withSpikeCounter) {// put the condition for runNetwork
					int bufPos = groupConfig[g].spkCntBufPos; // retrieve buf pos
					int bufNeur = i-groupConfig[g].StartN;
					spkCntBuf[bufPos][bufNeur]++;
				}
				spikeBufferFull = addSpikeToTable(i, g);

				if (spikeBufferFull)
					break;

				// STDP calculation: the post-synaptic neuron fires after the arrival of a pre-synaptic spike
				if (!sim_in_testing && groupConfig[g].WithSTDP) {
					unsigned int pos_ij = snnRuntimeData.cumulativePre[i]; // the index of pre-synaptic neuron
					for(int j=0; j < snnRuntimeData.Npre_plastic[i]; pos_ij++, j++) {
						int stdp_tDiff = (simTime-snnRuntimeData.synSpikeTime[pos_ij]);
						assert(!((stdp_tDiff < 0) && (snnRuntimeData.synSpikeTime[pos_ij] != MAX_SIMULATION_TIME)));

						if (stdp_tDiff > 0) {
							// check this is an excitatory or inhibitory synapse
							if (groupConfig[g].WithESTDP && snnRuntimeData.maxSynWt[pos_ij] >= 0) { // excitatory synapse
								// Handle E-STDP curve
								switch (groupConfig[g].WithESTDPcurve) {
								case EXP_CURVE: // exponential curve
									if (stdp_tDiff * groupConfig[g].TAU_PLUS_INV_EXC < 25)
										snnRuntimeData.wtChange[pos_ij] += STDP(stdp_tDiff, groupConfig[g].ALPHA_PLUS_EXC, groupConfig[g].TAU_PLUS_INV_EXC);
									break;
								case TIMING_BASED_CURVE: // sc curve
									if (stdp_tDiff * groupConfig[g].TAU_PLUS_INV_EXC < 25) {
										if (stdp_tDiff <= groupConfig[g].GAMMA)
											snnRuntimeData.wtChange[pos_ij] += groupConfig[g].OMEGA + groupConfig[g].KAPPA * STDP(stdp_tDiff, groupConfig[g].ALPHA_PLUS_EXC, groupConfig[g].TAU_PLUS_INV_EXC);
										else // stdp_tDiff > GAMMA
											snnRuntimeData.wtChange[pos_ij] -= STDP(stdp_tDiff, groupConfig[g].ALPHA_PLUS_EXC, groupConfig[g].TAU_PLUS_INV_EXC);
									}
									break;
								default:
									KERNEL_ERROR("Invalid E-STDP curve!");
									break;
								}
							} else if (groupConfig[g].WithISTDP && snnRuntimeData.maxSynWt[pos_ij] < 0) { // inhibitory synapse
								// Handle I-STDP curve
								switch (groupConfig[g].WithISTDPcurve) {
								case EXP_CURVE: // exponential curve
									if (stdp_tDiff * groupConfig[g].TAU_PLUS_INV_INB < 25) { // LTP of inhibitory synapse, which decreases synapse weight
										snnRuntimeData.wtChange[pos_ij] -= STDP(stdp_tDiff, groupConfig[g].ALPHA_PLUS_INB, groupConfig[g].TAU_PLUS_INV_INB);
									}
									break;
								case PULSE_CURVE: // pulse curve
									if (stdp_tDiff <= groupConfig[g].LAMBDA) { // LTP of inhibitory synapse, which decreases synapse weight
										snnRuntimeData.wtChange[pos_ij] -= groupConfig[g].BETA_LTP;
										//printf("I-STDP LTP\n");
									} else if (stdp_tDiff <= groupConfig[g].DELTA) { // LTD of inhibitory syanpse, which increase sysnapse weight
										snnRuntimeData.wtChange[pos_ij] -= groupConfig[g].BETA_LTD;
										//printf("I-STDP LTD\n");
									} else { /*do nothing*/}
									break;
								default:
									KERNEL_ERROR("Invalid I-STDP curve!");
									break;
								}
							}
						}
					}
				}
				spikeCountSec++;
			}
		}
	}
}

void SNN::generatePostSpike(unsigned int pre_i, unsigned int idx_d, unsigned int offset, int tD) {
	// get synaptic info...
	SynInfo post_info = snnRuntimeData.postSynapticIds[offset + idx_d];

	// get post-neuron id
	unsigned int post_i = GET_CONN_NEURON_ID(post_info);
	assert(post_i<numN);

	// get syn id
	int s_i = GET_CONN_SYN_ID(post_info);
	assert(s_i<(snnRuntimeData.Npre[post_i]));

	// get the cumulative position for quick access
	unsigned int pos_i = snnRuntimeData.cumulativePre[post_i] + s_i;
	assert(post_i < numNReg); // \FIXME is this assert supposed to be for pos_i?

	// get group id of pre- / post-neuron
	short int post_grpId = snnRuntimeData.grpIds[post_i];
	short int pre_grpId = snnRuntimeData.grpIds[pre_i];

	unsigned int pre_type = groupConfig[pre_grpId].Type;

	// get connect info from the cumulative synapse index for mulSynFast/mulSynSlow (requires less memory than storing
	// mulSynFast/Slow per synapse or storing a pointer to grpConnectInfo_s)
	// mulSynFast will be applied to fast currents (either AMPA or GABAa)
	// mulSynSlow will be applied to slow currents (either NMDA or GABAb)
	short int mulIndex = snnRuntimeData.connIdsPreIdx[pos_i];
	assert(mulIndex>=0 && mulIndex<numConnections);


	// for each presynaptic spike, postsynaptic (synaptic) current is going to increase by some amplitude (change)
	// generally speaking, this amplitude is the weight; but it can be modulated by STP
	float change = snnRuntimeData.wt[pos_i];

	if (groupConfig[pre_grpId].WithSTP) {
		// if pre-group has STP enabled, we need to modulate the weight
		// NOTE: Order is important! (Tsodyks & Markram, 1998; Mongillo, Barak, & Tsodyks, 2008)
		// use u^+ (value right after spike-update) but x^- (value right before spike-update)

		// dI/dt = -I/tau_S + A * u^+ * x^- * \delta(t-t_{spk})
		// I noticed that for connect(.., RangeDelay(1), ..) tD will be 0
		int ind_minus = STP_BUF_POS(pre_i,(simTime-tD-1));
		int ind_plus  = STP_BUF_POS(pre_i,(simTime-tD));

		change *= groupConfig[pre_grpId].STP_A*snnRuntimeData.stpu[ind_plus]*snnRuntimeData.stpx[ind_minus];

//		fprintf(stderr,"%d: %d[%d], numN=%d, td=%d, maxDelay_=%d, ind-=%d, ind+=%d, stpu=[%f,%f], stpx=[%f,%f], change=%f, wt=%f\n",
//			simTime, pre_grpId, pre_i,
//					numN, tD, maxDelay_, ind_minus, ind_plus,
//					stpu[ind_minus], stpu[ind_plus], stpx[ind_minus], stpx[ind_plus], change, wt[pos_i]);
	}

	// update currents
	// NOTE: it's faster to += 0.0 rather than checking for zero and not updating
	if (sim_with_conductances) {
		if (pre_type & TARGET_AMPA) // if post_i expresses AMPAR
			snnRuntimeData.gAMPA [post_i] += change*mulSynFast[mulIndex]; // scale by some factor
		if (pre_type & TARGET_NMDA) {
			if (sim_with_NMDA_rise) {
				snnRuntimeData.gNMDA_r[post_i] += change*sNMDA*mulSynSlow[mulIndex];
				snnRuntimeData.gNMDA_d[post_i] += change*sNMDA*mulSynSlow[mulIndex];
			} else {
				snnRuntimeData.gNMDA [post_i] += change*mulSynSlow[mulIndex];
			}
		}
		if (pre_type & TARGET_GABAa)
			snnRuntimeData.gGABAa[post_i] -= change*mulSynFast[mulIndex]; // wt should be negative for GABAa and GABAb
		if (pre_type & TARGET_GABAb) {
			if (sim_with_GABAb_rise) {
				snnRuntimeData.gGABAb_r[post_i] -= change*sGABAb*mulSynSlow[mulIndex];
				snnRuntimeData.gGABAb_d[post_i] -= change*sGABAb*mulSynSlow[mulIndex];
			} else {
				snnRuntimeData.gGABAb[post_i] -= change*mulSynSlow[mulIndex];
			}
		}
	} else {
		snnRuntimeData.current[post_i] += change;
	}

	snnRuntimeData.synSpikeTime[pos_i] = simTime;

	// Got one spike from dopaminergic neuron, increase dopamine concentration in the target area
	if (pre_type & TARGET_DA) {
		snnRuntimeData.grpDA[post_grpId] += 0.04;
	}

	// STDP calculation: the post-synaptic neuron fires before the arrival of a pre-synaptic spike
	if (!sim_in_testing && groupConfig[post_grpId].WithSTDP) {
		int stdp_tDiff = (simTime-snnRuntimeData.lastSpikeTime[post_i]);

		if (stdp_tDiff >= 0) {
			if (groupConfig[post_grpId].WithISTDP && ((pre_type & TARGET_GABAa) || (pre_type & TARGET_GABAb))) { // inhibitory syanpse
				// Handle I-STDP curve
				switch (groupConfig[post_grpId].WithISTDPcurve) {
				case EXP_CURVE: // exponential curve
					if ((stdp_tDiff*groupConfig[post_grpId].TAU_MINUS_INV_INB)<25) { // LTD of inhibitory syanpse, which increase synapse weight
						snnRuntimeData.wtChange[pos_i] -= STDP(stdp_tDiff, groupConfig[post_grpId].ALPHA_MINUS_INB, groupConfig[post_grpId].TAU_MINUS_INV_INB);
					}
					break;
				case PULSE_CURVE: // pulse curve
					if (stdp_tDiff <= groupConfig[post_grpId].LAMBDA) { // LTP of inhibitory synapse, which decreases synapse weight
						snnRuntimeData.wtChange[pos_i] -= groupConfig[post_grpId].BETA_LTP;
					} else if (stdp_tDiff <= groupConfig[post_grpId].DELTA) { // LTD of inhibitory syanpse, which increase synapse weight
						snnRuntimeData.wtChange[pos_i] -= groupConfig[post_grpId].BETA_LTD;
					} else { /*do nothing*/ }
					break;
				default:
					KERNEL_ERROR("Invalid I-STDP curve");
					break;
				}
			} else if (groupConfig[post_grpId].WithESTDP && ((pre_type & TARGET_AMPA) || (pre_type & TARGET_NMDA))) { // excitatory synapse
				// Handle E-STDP curve
				switch (groupConfig[post_grpId].WithESTDPcurve) {
				case EXP_CURVE: // exponential curve
				case TIMING_BASED_CURVE: // sc curve
					if (stdp_tDiff * groupConfig[post_grpId].TAU_MINUS_INV_EXC < 25)
						snnRuntimeData.wtChange[pos_i] += STDP(stdp_tDiff, groupConfig[post_grpId].ALPHA_MINUS_EXC, groupConfig[post_grpId].TAU_MINUS_INV_EXC);
					break;
				default:
					KERNEL_ERROR("Invalid E-STDP curve");
					break;
				}
			} else { /*do nothing*/ }
		}
		assert(!((stdp_tDiff < 0) && (snnRuntimeData.lastSpikeTime[post_i] != MAX_SIMULATION_TIME)));
	}
}

void SNN::generateSpikes() {
	PropagatedSpikeBuffer::const_iterator srg_iter;
	PropagatedSpikeBuffer::const_iterator srg_iter_end = pbuf->endSpikeTargetGroups();

	for( srg_iter = pbuf->beginSpikeTargetGroups(); srg_iter != srg_iter_end; ++srg_iter )  {
		// Get the target neurons for the given groupId
		int nid	 = srg_iter->stg;
		//delaystep_t del = srg_iter->delay;
		//generate a spike to all the target neurons from source neuron nid with a delay of del
		short int g = snnRuntimeData.grpIds[nid];

		addSpikeToTable (nid, g);
		spikeCountSec++;
		nPoissonSpikes++;
	}

	// advance the time step to the next phase...
	pbuf->nextTimeStep();
}

void SNN::generateSpikesFromFuncPtr(int grpId) {
	// \FIXME this function is a mess
	bool done;
	SpikeGeneratorCore* spikeGen = groupConfig[grpId].spikeGen;
	int timeSlice = groupConfig[grpId].CurrTimeSlice;
	int currTime = simTime;
	int spikeCnt = 0;
	for(int i = groupConfig[grpId].StartN; i <= groupConfig[grpId].EndN; i++) {
		// start the time from the last time it spiked, that way we can ensure that the refractory period is maintained
		int nextTime = snnRuntimeData.lastSpikeTime[i];
		if (nextTime == MAX_SIMULATION_TIME)
			nextTime = 0;

		// the end of the valid time window is either the length of the scheduling time slice from now (because that
		// is the max of the allowed propagated buffer size) or simply the end of the simulation
		int endOfTimeWindow = MIN(currTime+timeSlice,simTimeRunStop);

		done = false;
		while (!done) {
			// generate the next spike time (nextSchedTime) from the nextSpikeTime callback
			int nextSchedTime = spikeGen->nextSpikeTime(this, grpId, i - groupConfig[grpId].StartN, currTime, 
				nextTime, endOfTimeWindow);

			// the generated spike time is valid only if:
			// - it has not been scheduled before (nextSchedTime > nextTime)
			//    - but careful: we would drop spikes at t=0, because we cannot initialize nextTime to -1...
			// - it is within the scheduling time slice (nextSchedTime < endOfTimeWindow)
			// - it is not in the past (nextSchedTime >= currTime)
			if ((nextSchedTime==0 || nextSchedTime>nextTime) && nextSchedTime<endOfTimeWindow && nextSchedTime>=currTime) {
//				fprintf(stderr,"%u: spike scheduled for %d at %u\n",currTime, i-groupConfig[grpId].StartN,nextSchedTime);
				// scheduled spike...
				// \TODO CPU mode does not check whether the same AER event has been scheduled before (bug #212)
				// check how GPU mode does it, then do the same here.
				nextTime = nextSchedTime;
				pbuf->scheduleSpikeTargetGroup(i, nextTime - currTime);
				spikeCnt++;

				// update number of spikes if SpikeCounter set
				if (groupConfig[grpId].withSpikeCounter) {
					int bufPos = groupConfig[grpId].spkCntBufPos; // retrieve buf pos
					int bufNeur = i-groupConfig[grpId].StartN;
					spkCntBuf[bufPos][bufNeur]++;
				}
			} else {
				done = true;
			}
		}
	}
}

void SNN::generateSpikesFromRate(int grpId) {
	bool done;
	PoissonRate* rate = groupConfig[grpId].RatePtr;
	float refPeriod = groupConfig[grpId].RefractPeriod;
	int timeSlice   = groupConfig[grpId].CurrTimeSlice;
	int currTime = simTime;
	int spikeCnt = 0;

	if (rate == NULL)
		return;

	if (rate->isOnGPU()) {
		KERNEL_ERROR("Specifying rates on the GPU but using the CPU SNN is not supported.");
		exitSimulation(1);
	}

	const int nNeur = rate->getNumNeurons();
	if (nNeur != groupConfig[grpId].SizeN) {
		KERNEL_ERROR("Length of PoissonRate array (%d) did not match number of neurons (%d) for group %d(%s).",
			nNeur, groupConfig[grpId].SizeN, grpId, getGroupName(grpId).c_str());
		exitSimulation(1);
	}

	for (int neurId=0; neurId<nNeur; neurId++) {
		float frate = rate->getRate(neurId);

		// start the time from the last time it spiked, that way we can ensure that the refractory period is maintained
		int nextTime = snnRuntimeData.lastSpikeTime[groupConfig[grpId].StartN + neurId];
		if (nextTime == MAX_SIMULATION_TIME)
			nextTime = 0;

		done = false;
		while (!done && frate>0) {
			nextTime = poissonSpike(nextTime, frate/1000.0, refPeriod);
			// found a valid timeSlice
			if (nextTime < (currTime+timeSlice)) {
				if (nextTime >= currTime) {
//					int nid = groupConfig[grpId].StartN+cnt;
					pbuf->scheduleSpikeTargetGroup(groupConfig[grpId].StartN + neurId, nextTime-currTime);
					spikeCnt++;

					// update number of spikes if SpikeCounter set
					if (groupConfig[grpId].withSpikeCounter) {
						int bufPos = groupConfig[grpId].spkCntBufPos; // retrieve buf pos
						spkCntBuf[bufPos][neurId]++;
					}
				}
			}
			else {
				done=true;
			}
		}
	}
}

void  SNN::globalStateUpdate() {
	double tmp_iNMDA, tmp_I;
	double tmp_gNMDA, tmp_gGABAb;

	for(int g=0; g < numGroups; g++) {
		if (groupConfig[g].Type&POISSON_NEURON) {
			if (groupConfig[g].WithHomeostasis) {
				for(int i=groupConfig[g].StartN; i <= groupConfig[g].EndN; i++)
					snnRuntimeData.avgFiring[i] *= groupConfig[g].avgTimeScale_decay;
			}
			continue;
		}

		// decay dopamine concentration
		if ((groupConfig[g].WithESTDPtype == DA_MOD || groupConfig[g].WithISTDP == DA_MOD) && snnRuntimeData.grpDA[g] > groupConfig[g].baseDP) {
			snnRuntimeData.grpDA[g] *= groupConfig[g].decayDP;
		}
		snnRuntimeData.grpDABuffer[g][simTimeMs] = snnRuntimeData.grpDA[g];

		for(int i=groupConfig[g].StartN; i <= groupConfig[g].EndN; i++) {
			assert(i < numNReg);
			// update average firing rate for homeostasis
			if (groupConfig[g].WithHomeostasis)
				snnRuntimeData.avgFiring[i] *= groupConfig[g].avgTimeScale_decay;

			// update conductances
			if (sim_with_conductances) {
				// COBA model

				// all the tmpIs will be summed into current[i] in the following loop
				snnRuntimeData.current[i] = 0.0f;

				// \FIXME: these tmp vars cause a lot of rounding errors... consider rewriting
				for (int j=0; j<COND_INTEGRATION_SCALE; j++) {
					tmp_iNMDA = (snnRuntimeData.voltage[i]+80.0)*(snnRuntimeData.voltage[i]+80.0)/60.0/60.0;

					tmp_gNMDA = sim_with_NMDA_rise ? snnRuntimeData.gNMDA_d[i]-snnRuntimeData.gNMDA_r[i] : snnRuntimeData.gNMDA[i];
					tmp_gGABAb = sim_with_GABAb_rise ? snnRuntimeData.gGABAb_d[i]-snnRuntimeData.gGABAb_r[i] : snnRuntimeData.gGABAb[i];

					tmp_I = -(   snnRuntimeData.gAMPA[i]*(snnRuntimeData.voltage[i]-0)
									 + tmp_gNMDA*tmp_iNMDA/(1+tmp_iNMDA)*(snnRuntimeData.voltage[i]-0)
									 + snnRuntimeData.gGABAa[i]*(snnRuntimeData.voltage[i]+70)
									 + tmp_gGABAb*(snnRuntimeData.voltage[i]+90)
								   );

					snnRuntimeData.voltage[i] += ((0.04*snnRuntimeData.voltage[i]+5.0)*snnRuntimeData.voltage[i]+140.0-snnRuntimeData.recovery[i]+tmp_I+snnRuntimeData.extCurrent[i])
						/ COND_INTEGRATION_SCALE;
					assert(!isnan(snnRuntimeData.voltage[i]) && !isinf(snnRuntimeData.voltage[i]));

					// keep track of total current
					snnRuntimeData.current[i] += tmp_I;

					if (snnRuntimeData.voltage[i] > 30) {
						snnRuntimeData.voltage[i] = 30;
						j=COND_INTEGRATION_SCALE; // break the loop but evaluate u[i]
//						if (gNMDA[i]>=10.0f) KERNEL_WARN("High NMDA conductance (gNMDA>=10.0) may cause instability");
//						if (gGABAb[i]>=2.0f) KERNEL_WARN("High GABAb conductance (gGABAb>=2.0) may cause instability");
					}
					if (snnRuntimeData.voltage[i] < -90)
						snnRuntimeData.voltage[i] = -90;
					snnRuntimeData.recovery[i]+=snnRuntimeData.Izh_a[i]*(snnRuntimeData.Izh_b[i]*snnRuntimeData.voltage[i]-snnRuntimeData.recovery[i])/COND_INTEGRATION_SCALE;
				} // end COND_INTEGRATION_SCALE loop
			} else {
				// CUBA model
				snnRuntimeData.voltage[i] += 0.5*((0.04*snnRuntimeData.voltage[i]+5.0)*snnRuntimeData.voltage[i] + 140.0 - snnRuntimeData.recovery[i]
					+ snnRuntimeData.current[i] + snnRuntimeData.extCurrent[i]); //for numerical stability
				snnRuntimeData.voltage[i] += 0.5*((0.04*snnRuntimeData.voltage[i]+5.0)*snnRuntimeData.voltage[i] + 140.0 - snnRuntimeData.recovery[i]
					+ snnRuntimeData.current[i] + snnRuntimeData.extCurrent[i]); //time step is 0.5 ms
				if (snnRuntimeData.voltage[i] > 30)
					snnRuntimeData.voltage[i] = 30;
				if (snnRuntimeData.voltage[i] < -90)
					snnRuntimeData.voltage[i] = -90;
				snnRuntimeData.recovery[i]+=snnRuntimeData.Izh_a[i]*(snnRuntimeData.Izh_b[i]*snnRuntimeData.voltage[i]-snnRuntimeData.recovery[i]);
			} // end COBA/CUBA
		} // end StartN...EndN
	} // end numGroups
}

// This function updates the synaptic weights from its derivatives..
void SNN::updateWeights() {
	// at this point we have already checked for sim_in_testing and sim_with_fixedwts
	assert(sim_in_testing==false);
	assert(sim_with_fixedwts==false);

	// update synaptic weights here for all the neurons..
	for(int g = 0; g < numGroups; g++) {
		// no changable weights so continue without changing..
		if(groupConfig[g].FixedInputWts || !(groupConfig[g].WithSTDP))
			continue;

		for(int i = groupConfig[g].StartN; i <= groupConfig[g].EndN; i++) {
			assert(i < numNReg);
			unsigned int offset = snnRuntimeData.cumulativePre[i];
			float diff_firing = 0.0;
			float homeostasisScale = 1.0;

			if(groupConfig[g].WithHomeostasis) {
				assert(snnRuntimeData.baseFiring[i]>0);
				diff_firing = 1-snnRuntimeData.avgFiring[i]/snnRuntimeData.baseFiring[i];
				homeostasisScale = groupConfig[g].homeostasisScale;
			}

			if (i==groupConfig[g].StartN)
				KERNEL_DEBUG("Weights, Change at %lu (diff_firing: %f)", simTimeSec, diff_firing);

			for(int j = 0; j < snnRuntimeData.Npre_plastic[i]; j++) {
				//	if (i==groupConfig[g].StartN)
				//		KERNEL_DEBUG("%1.2f %1.2f \t", wt[offset+j]*10, wtChange[offset+j]*10);
				float effectiveWtChange = stdpScaleFactor_ * snnRuntimeData.wtChange[offset + j];
//				if (wtChange[offset+j])
//					printf("connId=%d, wtChange[%d]=%f\n",connIdsPreIdx[offset+j],offset+j,wtChange[offset+j]);

				// homeostatic weight update
				// FIXME: check WithESTDPtype and WithISTDPtype first and then do weight change update
				switch (groupConfig[g].WithESTDPtype) {
				case STANDARD:
					if (groupConfig[g].WithHomeostasis) {
						snnRuntimeData.wt[offset+j] += (diff_firing*snnRuntimeData.wt[offset+j]*homeostasisScale + snnRuntimeData.wtChange[offset+j])*snnRuntimeData.baseFiring[i]/groupConfig[g].avgTimeScale/(1+fabs(diff_firing)*50);
					} else {
						// just STDP weight update
						snnRuntimeData.wt[offset+j] += effectiveWtChange;
					}
					break;
				case DA_MOD:
					if (groupConfig[g].WithHomeostasis) {
						effectiveWtChange = snnRuntimeData.grpDA[g] * effectiveWtChange;
						snnRuntimeData.wt[offset+j] += (diff_firing*snnRuntimeData.wt[offset+j]*homeostasisScale + effectiveWtChange)*snnRuntimeData.baseFiring[i]/groupConfig[g].avgTimeScale/(1+fabs(diff_firing)*50);
					} else {
						snnRuntimeData.wt[offset+j] += snnRuntimeData.grpDA[g] * effectiveWtChange;
					}
					break;
				case UNKNOWN_STDP:
				default:
					// we shouldn't even be in here if !WithSTDP
					break;
				}

				switch (groupConfig[g].WithISTDPtype) {
				case STANDARD:
					if (groupConfig[g].WithHomeostasis) {
						snnRuntimeData.wt[offset+j] += (diff_firing*snnRuntimeData.wt[offset+j]*homeostasisScale + snnRuntimeData.wtChange[offset+j])*snnRuntimeData.baseFiring[i]/groupConfig[g].avgTimeScale/(1+fabs(diff_firing)*50);
					} else {
						// just STDP weight update
						snnRuntimeData.wt[offset+j] += effectiveWtChange;
					}
					break;
				case DA_MOD:
					if (groupConfig[g].WithHomeostasis) {
						effectiveWtChange = snnRuntimeData.grpDA[g] * effectiveWtChange;
						snnRuntimeData.wt[offset+j] += (diff_firing*snnRuntimeData.wt[offset+j]*homeostasisScale + effectiveWtChange)*snnRuntimeData.baseFiring[i]/groupConfig[g].avgTimeScale/(1+fabs(diff_firing)*50);
					} else {
						snnRuntimeData.wt[offset+j] += snnRuntimeData.grpDA[g] * effectiveWtChange;
					}
					break;
				case UNKNOWN_STDP:
				default:
					// we shouldn't even be in here if !WithSTDP
					break;
				}

				// It is users' choice to decay weight change or not
				// see setWeightAndWeightChangeUpdate()
				snnRuntimeData.wtChange[offset+j] *= wtChangeDecay_;

				// if this is an excitatory or inhibitory synapse
				if (snnRuntimeData.maxSynWt[offset + j] >= 0) {
					if (snnRuntimeData.wt[offset + j] >= snnRuntimeData.maxSynWt[offset + j])
						snnRuntimeData.wt[offset + j] = snnRuntimeData.maxSynWt[offset + j];
					if (snnRuntimeData.wt[offset + j] < 0)
						snnRuntimeData.wt[offset + j] = 0.0;
				} else {
					if (snnRuntimeData.wt[offset + j] <= snnRuntimeData.maxSynWt[offset + j])
						snnRuntimeData.wt[offset + j] = snnRuntimeData.maxSynWt[offset + j];
					if (snnRuntimeData.wt[offset+j] > 0)
						snnRuntimeData.wt[offset+j] = 0.0;
				}
			}
		}
	}
}

/*!
 * \brief This function is called every second by SNN::runNetwork(). It updates the firingTableD1(D2) and
 * timeTableD1(D2) by removing older firing information.
 */
void SNN::shiftSpikeTables() {
	// Read the neuron ids that fired in the last maxDelay_ seconds
	// and put it to the beginning of the firing table...
	for(int p=timeTableD2[999],k=0;p<timeTableD2[999+maxDelay_+1];p++,k++) {
		firingTableD2[k] = firingTableD2[p];
	}

	for(int i=0; i < maxDelay_; i++) {
		timeTableD2[i+1] = timeTableD2[1000+i+1]-timeTableD2[1000];
	}

	timeTableD1[maxDelay_] = 0;

	/* the code of weight update has been moved to SNN::updateWeights() */

	spikeCount	+= spikeCountSec;
	spikeCountD2 += (spikeCountD2Sec-timeTableD2[maxDelay_]);
	spikeCountD1 += spikeCountD1Sec;

	spikeCountD1Sec  = 0;
	spikeCountSec = 0;
	spikeCountD2Sec = timeTableD2[maxDelay_];
}