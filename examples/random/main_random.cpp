/*
 * Copyright (c) 2013 Regents of the University of California. All rights reserved.
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
 * created by: 		(MDR) Micah Richert, (JN) Jayram M. Nageswaran
 * maintained by:	(MA) Mike Avery <averym@uci.edu>, (MB) Michael Beyeler <mbeyeler@uci.edu>,
 *					(KDC) Kristofor Carlson <kdcarlso@uci.edu>
 *
 * CARLsim available from http://socsci.uci.edu/~jkrichma/CARL/CARLsim/
 * Ver 3/22/14
 */

#include <carlsim.h>

#include <vector>
#include <stdio.h>

#if (WIN32 || WIN64)
	#define _CRT_SECURE_NO_WARNINGS
#endif

// record execution time
unsigned long int get_time_ms64() {
#ifdef WIN32
	/* Windows */
	FILETIME ft;
	LARGE_INTEGER li;

	/* Get the amount of 100 nano seconds intervals elapsed since January 1, 1601 (UTC) and copy it
	 * to a LARGE_INTEGER structure. */
	GetSystemTimeAsFileTime(&ft);
	li.LowPart = ft.dwLowDateTime;
	li.HighPart = ft.dwHighDateTime;

	unsigned long int ret = li.QuadPart;
	ret -= 116444736000000000LL; /* Convert from file time to UNIX epoch time. */
	ret /= 10000; /* From 100 nano seconds (10^-7) to 1 millisecond (10^-3) intervals */

 	return ret;
#else
	/* Linux */
	struct timeval tv;
 	gettimeofday(&tv, NULL);

	unsigned long int ret = tv.tv_usec;
	/* Convert from micro seconds (10^-6) to milliseconds (10^-3) */
	ret /= 1000;

	/* Adds the seconds (10^0) after converting them to milliseconds (10^-3) */
	ret += (tv.tv_sec * 1000);

	return ret;
#endif
}

int main() {
	// simulation details
	int N = 100000; // number of neurons
	int ithGPU = 0; // run on first GPU

	// create a network
	CARLsim sim("random", GPU_MODE, SILENT, ithGPU, 42);
	sim.setLogFile("results/carlsim.log");

	int g1=sim.createGroup("excit", N*0.8, EXCITATORY_NEURON);
	sim.setNeuronParameters(g1, 0.02f, 0.2f, -65.0f, 8.0f);

	int g2=sim.createGroup("inhib", N*0.2, INHIBITORY_NEURON);
	sim.setNeuronParameters(g2, 0.1f,  0.2f, -65.0f, 2.0f);

	int gin=sim.createSpikeGeneratorGroup("input",N*0.1,EXCITATORY_NEURON);

	sim.setConductances(true,5,150,6,150);

	float prob = 100.0f/N;
	// make random connections with 10% probability
	sim.connect(g2,g1,"random", RangeWeight(0.01), prob);
	// make random connections with 10% probability, and random delays between 1 and 20
	sim.connect(g1,g2,"random", RangeWeight(0.0,0.0025,0.005), prob, RangeDelay(1,20), RadiusRF(-1), SYN_PLASTIC);
	sim.connect(g1,g1,"random", RangeWeight(0.0,0.06,0.1), prob, RangeDelay(1,20), RadiusRF(-1), SYN_PLASTIC);

	// 5% probability of connection
	sim.connect(gin, g1, "random", RangeWeight(1.0), prob/10.0f, RangeDelay(1,20), RadiusRF(-1));

	// here we define and set the properties of the STDP.
	float ALPHA_LTP_EXC = 0.10f/100, TAU_LTP = 20.0f, ALPHA_LTD_EXC = 0.12f/100, TAU_LTD = 20.0f;
	sim.setSTDP(g1, true, STANDARD, ALPHA_LTP_EXC, TAU_LTP, ALPHA_LTD_EXC, TAU_LTD);
	sim.setSTDP(g2, true, STANDARD, ALPHA_LTP_EXC, TAU_LTP, ALPHA_LTD_EXC, TAU_LTD);

	// build the network
	sim.setupNetwork();

	// record spike times, save to binary
//	sim.setSpikeMonitor(g1, "Default");
//	sim.setSpikeMonitor(g2, "Default");
//	sim.setSpikeMonitor(gin, "Default");

	// record weights of g1->g1 connection, save to binary
//	sim.setConnectionMonitor(g1,g1, "Default");

	//setup some baseline input
	PoissonRate in(N*0.1);
	in.setRates(1.0f);
	sim.setSpikeRate(gin,&in);

	// run for a total of 10 seconds
	// at the end of each runNetwork call, Spike and Connection Monitor stats will be printed
	bool printRunStats = true;
	unsigned long int timeStart = get_time_ms64();
	sim.runNetwork(2,0);
	printf("\nExecution time: %ld ms\n\n",get_time_ms64()-timeStart);

	return 0;
}

