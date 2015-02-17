

#include <stdlib.h>
#include <assert.h>
#include <fstream>
#include <iostream>
#include <sstream>
using namespace std;
#include "cache.h"

ulong string_to_ulong(string a) {
	int i;
	ulong val = 0;
	int len = a.size();
	for (i = 0; i < len; i++) {
		if (a[i] >= 48 && a[i] <= 57)
			val += (a[i] - 48) * (1 << (4 * (len - 1 - i)));
		else if (a[i] >= 97 && a[i] <= 122)
			val += (a[i] - 87) * (1 << (4 * (len - 1 - i)));
		else if (a[i] >= 65 && a[i] <= 90)
			val += (a[i] - 55) * (1 << (4 * (len - 1 - i)));
	}
	return val;
}

int main(int argc, char *argv[]) {

	if (argv[1] == NULL) {
		printf("input format: ");
		printf(
				"./smp_cache <cache_size> <assoc> <block_size> <num_processors> <protocol> <trace_file> \n");
		exit(0);
	}

	/*****uncomment the next five lines*****/
	int cache_size = atoi(argv[1]);
	int cache_assoc = atoi(argv[2]);
	int blk_size = atoi(argv[3]);
	int num_processors = atoi(argv[4]);/*1, 2, 4, 8*/
	int protocol = atoi(argv[5]); /*0:MSI, 1:MESI, 2:MOESI*/
	string fname = argv[6];

	//*******print out simulator configuration here*******//
	cout << "===== 506 SMP Simulator Configuration =====\n";
	cout << "L1_SIZE:\t" << cache_size << endl;
	cout << "L1_ASSOC:\t" << cache_assoc << endl;
	cout << "L1_BLOCKSIZE:\t" << blk_size << endl;
	cout << "NUMBER OF PROCESSORS:\t" << num_processors << endl;
	if (protocol == 0)
	cout << "COHERENCE PROTOCOL:\t" << "MSI" << endl;
	else if (protocol == 1)
		cout << "COHERENCE PROTOCOL:\t" << "MESI" << endl;
	else if (protocol == 2)
	cout << "COHERENCE PROTOCOL:\t\t" << "Dragon" << endl;
	cout << "TRACE FILE:\t\t" << fname << endl;

	//*****create an array of caches here**********//

	Cache *caches[num_processors];
	for (int i = 0; i < num_processors; i++) {
		caches[i] = new Cache(cache_size, cache_assoc, blk_size, protocol);
	}

	string line;
	ifstream fin(fname.c_str());
	if (!fin.is_open()) {
		printf("Trace file problem\n");
		exit(0);
	}

	//**read trace file,line by line,each(processor#,operation,address)**//
	//*****propagate each request down through memory hierarchy**********//
	//*****by calling cachesArray[processor#]->Access(...)***************//

	else {

		while (getline(fin, line)) {
		

			istringstream iss(line);
			string s[3];
			int i = 0;
			while(i < 3) {
				iss >> s[i];
				i++;
			} 

			int proc_no = atoi(s[0].c_str()); //convert string into integer 
			char oper = *(s[1].c_str()); // convert string into character
			ulong address = string_to_ulong(s[2]); // convert string to unsigned long 
			int current_state = INVALID;
			int next_state = INVALID;
			int c2ctransfers = 0;
			bool flush = false;
			int flag = 0;
			cacheLine * current = caches[proc_no]->findLine(address);
			if (current != NULL)
				current_state = current->getFlags();
            // MSI protocol case // 
			if (protocol == 0) {
				switch (current_state) {
				case INVALID:
					for (int i = 0; i < num_processors; i++) {
						if (i != proc_no) {
							if (oper == 'r')
								caches[i]->busRd(address);
							else
								caches[i]->busRdx(address);
						}
					}
					if (oper == 'r') {
						next_state = SHARED;
						caches[proc_no]->mem_transac++;
					} else {
						next_state = MODIFIED;
						caches[proc_no]->mem_transac++;					
					}
					break;
				case SHARED:
					if (oper == 'w') {
						for (int i = 0; i < num_processors; i++) {
							if (i != proc_no)
								caches[i]->busRdx(address);
						}

						next_state = MODIFIED;
					    caches[proc_no]->mem_transac++;	
					} else
						next_state = current_state;
					break;
				case MODIFIED:
					next_state = current_state;
					break;
				}
            // MESI protocol case // 
			} else if (protocol == 1) {
				switch (current_state) {
				case INVALID:
					for (int i = 0; i < num_processors; i++) {
						if (i != proc_no) {
							if (oper == 'r')
								c2ctransfers = c2ctransfers
										+ caches[i]->busRd(address);
							else
								flush = flush | caches[i]->busRdx(address);
						}
					}
					if (oper == 'r') {
						if (c2ctransfers != 0) {
							next_state = SHARED;
							caches[proc_no]->cache2cache++;
						} else {
							next_state = EXCLUSIVE;
							caches[proc_no]->mem_transac++;
						}

					} else {
						next_state = MODIFIED;
						//caches[proc_no]->mem_transac++;
						if (flush){
							caches[proc_no]->cache2cache++;
                        }
                        else
                        {
							caches[proc_no]->mem_transac++;
						}
					}
					break;
			    case EXCLUSIVE:
					if (oper == 'w') {
						next_state = MODIFIED;
					} else
						next_state = current_state;
					break;
				case SHARED:
					if (oper == 'w') {
						for (int i = 0; i < num_processors; i++) {
							if (i != proc_no)
								caches[i]->busUpgr(address);
						}
						next_state = MODIFIED;
					} else
						next_state = current_state;
					break;
			
				case MODIFIED:
					next_state = current_state;
					break;
				}
				// Dragon protocol case // 
			}  else if (protocol == 2) {

				switch (current_state) {
				case INVALID:
					for (int i = 0; i < num_processors; i++) {
						if (i != proc_no) {
							flag = flag + caches[i]->busRd(address);
						}
					}
					if (oper == 'r') {
						if (flag != 0) {
							next_state = SHARED_CLEAN_DRAGON;
						} else {
							next_state = EXCLUSIVE_DRAGON;
						}

					} else {
						if (flag != 0) {
							next_state = SHARED_MODIFIED_DRAGON;

							for (int i = 0; i < num_processors; i++) {
								if (i != proc_no) {
									caches[i]->busUpdate(address);
								}
							}

						} else {
							next_state = MODIFIED_DRAGON;
						}

					}
					break;
                case EXCLUSIVE_DRAGON:
                      if (oper == 'w') {
						next_state = MODIFIED_DRAGON;
					} else
						next_state = current_state;
					break;
				case SHARED_CLEAN_DRAGON:
					if (oper == 'w') {
						for (int i = 0; i < num_processors; i++) {
							if (i != proc_no)
								flush = flush | caches[i]->busUpdate(address);
						}
						if (flush)
							next_state = SHARED_MODIFIED_DRAGON;
						else {
							next_state = MODIFIED_DRAGON;
						}
					} else
						next_state = current_state;
					break;

				case SHARED_MODIFIED_DRAGON:
				    if(oper == 'w'){
				    	for(int i=0; i < num_processors; i++){
				    		if(i != proc_no)
				    			flush = flush | caches[i]->busUpdate(address);
				    	}
				    	if(flush)
				    	{
				          next_state = current_state;
				    	}
				    	else
				    	{
				    		next_state = MODIFIED_DRAGON;
				    	}
				    }
				    else{
				    	next_state = current_state;
				    }
				case MODIFIED_DRAGON:
					next_state = current_state;
					break;
				}
			}
			caches[proc_no]->Access(address, oper, next_state);
		}
		fin.close();

//********************************//
//print out all caches' statistics //
//********************************//
		for (int i = 0; i < num_processors; i++) {
			cout << "===== Simulation results (Cache_" << i << ")      =====" << endl;
			caches[i]->printStats();
		}
	}
}
