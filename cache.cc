

#include <stdlib.h>
#include <assert.h>
#include "cache.h"
#include <stdio.h>
using namespace std;

Cache::Cache(int s, int a, int b, int p) {
	ulong i, j;
	reads = readMisses = writes = 0;
	writeMisses = writeBacks = currentCycle = 0;
	protocol = p;

	cache2cache = 0;
	mem_transac = 0;
	interventions = 0;
	invalidations = 0;
	flushes = 0;

	size = (ulong) (s);
	lineSize = (ulong) (b);
	assoc = (ulong) (a);
	sets = (ulong) ((s / b) / a);
	numLines = (ulong) (s / b);
	log2Sets = (ulong) (log2(sets));
	log2Blk = (ulong) (log2(b));

	//initialize your counters here//

	tagMask = 0;
	for (i = 0; i < log2Sets; i++) {
		tagMask <<= 1;
		tagMask |= 1;
	}

	/**create a two dimentional cache, sized as cache[sets][assoc]**/
	cache = new cacheLine*[sets];
	for (i = 0; i < sets; i++) {
		cache[i] = new cacheLine[assoc];
		for (j = 0; j < assoc; j++) {
			cache[i][j].invalidate();
		}
	}

}

/**you might add other parameters to Access()
 since this function is an entry point
 to the memory hierarchy (i.e. caches)**/
void Cache::Access(ulong addr, uchar op, ulong state) {
	currentCycle++;/*per cache global counter to maintain LRU order 
	 among cache ways, updated on every cache access*/

	if (op == 'w')
		writes++;
	else
		reads++;

	cacheLine * line = findLine(addr);
	if (line == NULL)/*miss*/
	{
		if (op == 'w')
			writeMisses++;
		else
			readMisses++;

		cacheLine *newline = fillLine(addr);
		//if(op == 'w') newline->setFlags(DIRTY);
		newline->setFlags(state);

	} else {
		/**since it's a hit, update LRU and update dirty flag**/
		updateLRU(line);
		//if(op == 'w') line->setFlags(DIRTY);
		line->setFlags(state);

	}
}

/*look up line*/
cacheLine * Cache::findLine(ulong addr) {
	ulong i, j, tag, pos;

	pos = assoc;
	tag = calcTag(addr);
	i = calcIndex(addr);

	for (j = 0; j < assoc; j++)
		if (cache[i][j].isValid())
			if (cache[i][j].getTag() == tag) {
				pos = j;
				break;
			}
	if (pos == assoc)
		return NULL;
	else
		return &(cache[i][pos]);
}

/*upgrade LRU line to be MRU line*/
void Cache::updateLRU(cacheLine *line) {
	line->setSeq(currentCycle);
}

/*return an invalid line as LRU, if any, otherwise return LRU line*/
cacheLine * Cache::getLRU(ulong addr) {
	ulong i, j, victim, min;

	victim = assoc;
	min = currentCycle;
	i = calcIndex(addr);

	for (j = 0; j < assoc; j++) {
		if (cache[i][j].isValid() == 0)
			return &(cache[i][j]);
	}
	for (j = 0; j < assoc; j++) {
		if (cache[i][j].getSeq() <= min) {
			victim = j;
			min = cache[i][j].getSeq();
		}
	}
	assert(victim != assoc);

	return &(cache[i][victim]);
}

/*find a victim, move it to MRU position*/
cacheLine *Cache::findLineToReplace(ulong addr) {
	cacheLine * victim = getLRU(addr);
	updateLRU(victim);

	return (victim);
}

/*allocate a new line*/
cacheLine *Cache::fillLine(ulong addr) {
	ulong tag;

	cacheLine *victim = findLineToReplace(addr);
	assert(victim != 0);
	if (victim->getFlags() == MODIFIED)
		writeBack(addr);
    if(victim->getFlags() == MODIFIED_DRAGON)
    	writeBack(addr);
    if(victim->getFlags() == SHARED_MODIFIED_DRAGON)
    	writeBack(addr);
	tag = calcTag(addr);
	victim->setTag(tag);
	//victim->setFlags(VALID);
	/**note that this cache line has been already
	 upgraded to MRU in the previous function (findLineToReplace)**/

	return victim;
}

int Cache::busRd(ulong addr) {
	int current_state = INVALID;
	cacheLine *current = findLine(addr);
	if (current != NULL)
		current_state = current->getFlags();

	if (protocol == 0) {
		switch (current_state) {
		case INVALID:
			return 0;
		case SHARED:
			return 0;
		case MODIFIED:
			current->setFlags(SHARED);
			writeBack(addr);
			interventions++;
			flushes++;
			return 0;
		}

	} else if (protocol == 1) {
		switch (current_state) {
		case INVALID:
			return 0;
		case SHARED:
			return 1;
		case EXCLUSIVE:
			current->setFlags(SHARED);
			interventions++;
			return 1;
		case MODIFIED:
			current->setFlags(SHARED);
			writeBack(addr);
			flushes++;
			interventions++;
			return 1;
		}
	}  else if (protocol == 2) {

		switch (current_state) {
		case INVALID:
			return 0;	
		case EXCLUSIVE_DRAGON:
			current->setFlags(SHARED_CLEAN_DRAGON);
			interventions++;
			return 1;

        case SHARED_CLEAN_DRAGON:
             return 1;

        case SHARED_MODIFIED_DRAGON:
            writeBack(addr);
            flushes++;
			return 1;

		case MODIFIED_DRAGON:
			current->setFlags(SHARED_MODIFIED_DRAGON);
			writeBack(addr);
			interventions++;
			mem_transac++;
			flushes++;
			return 1;

		
		}
		
	}
return 0;
}

int Cache::busRdx(ulong addr) {
	int current_state = INVALID;
	cacheLine *current = findLine(addr);
	if (current != NULL)
		current_state = current->getFlags();

	if (protocol == 0) {
		switch (current_state) {
		case INVALID:
			return 0;
		case SHARED:
			current->setFlags(INVALID);
			invalidations++;
			break;
		case MODIFIED:
			current->setFlags(INVALID);
			writeBack(addr);
			invalidations++;
			flushes++;
			break;
		}

	} else if (protocol == 1) {
		switch (current_state) {
		case INVALID:
			return 0;
		case SHARED:
			current->setFlags(INVALID);
			invalidations++;
			return 1;
		case EXCLUSIVE:
			current->setFlags(INVALID);
			invalidations++;
			return 1;
		case MODIFIED:
			current->setFlags(INVALID);
			writeBack(addr);
			flushes++;
			invalidations++;
			return 1;
		}
	} 
	return 0;
}

void Cache::busUpgr(ulong addr) {
	int current_state = INVALID;
	cacheLine *current = findLine(addr);
	if (current != NULL)
		current_state = current->getFlags();

	if (protocol == 1) {
		switch (current_state) {
		case SHARED:
			current->setFlags(INVALID);
			invalidations++;
			return;
		}
	} 
}

int Cache::busUpdate(ulong addr) {
	int current_state = INVALID;
	cacheLine *current = findLine(addr);

	if (current != NULL)
		current_state = current->getFlags();
	else
		return 0;

	if (current_state == SHARED)
		return 1;

	if(protocol == 2)
	{
		switch(current_state){
			case SHARED_CLEAN_DRAGON:
			      return 1;
			case SHARED_MODIFIED_DRAGON:
			     current->setFlags(SHARED_CLEAN_DRAGON);
			      return 1;
		}
	}

	return 0;
}


void Cache::printStats() {
	if(protocol == 0 || protocol == 1)
	{
       mem_transac = mem_transac + getWB();
	}

	if(protocol == 2)
	{
		mem_transac = getRM() + getWM() + getWB();
	}

	cout << "01. number of reads:	 " << getReads() << endl;
	cout << "02. number of read misses:	 " << getRM() << endl;
	cout << "03. number of writes:	 " << getWrites() << endl;
	cout << "04. number of write misses:	 " << getWM() << endl;
	cout << "05. total miss rate:   "<< getMR() << endl;
	cout << "06. number of write backs:	 " << getWB() << endl;
	cout << "07. number of cache to cache transfers:	 " << cache2cache << endl;
	cout << "08. number of memory transactions:   " << mem_transac << endl;
	cout << "09. number of interventions:	 " << interventions << endl;
	cout << "10. number of invalidations:	 " << invalidations << endl;
	cout << "11. number of flushes:	 " << flushes << endl;
}
