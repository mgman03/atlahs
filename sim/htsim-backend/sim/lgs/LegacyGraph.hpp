#pragma once

// Original graph builder and serializer, used by the legacy txt2bin converter only.
#include <limits>
#include <map>
#include <memory>
#include <vector>
#include <string>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <inttypes.h>
#include <sys/mman.h>
#include <sys/types.h>

#include "LogGOPSim.hpp"

#define MAGIC_COOKIE 4223


#define OPTYPE_SEND 1
#define OPTYPE_RECV 2
#define OPTYPE_CALC 3

typedef uint64_t base_t;

// TODO Experiment with packing of the node structure,
//      that could save us up to 30% in space, but might
//      harm time.

struct Node {
	
	uint64_t           Size;
	std::vector<Node*> DependOnMe;
	std::vector<Node*> StartDependOnMe;
	uint32_t           DependenciesCnt;
	uint32_t           offset;
	uint32_t           Peer;
	uint32_t           Tag;
	uint8_t            Proc;
	uint8_t            Nic;
	char               Type;
	
};


class Graph {
	
	private:
	
	std::vector<Node*> RootNodes;
	std::vector<Node*> allNodes;

	//uint32_t num_nodes;
	uint32_t num_edges;
	uint32_t offset_cntr;

	char* mapping_start;

/*
	uint64_t get_file_size(int fd) {
		
		struct stat f_info;
		int r = fstat(fd, &f_info);
		assert(r == 0);
		return f_info.st_size;
	}
*/

	void find_root_nodes() {

		RootNodes.clear();

		for (std::vector<Node*>::iterator it =  allNodes.begin(); it != allNodes.end(); it++) {
			if ((**it).DependenciesCnt == 0) RootNodes.push_back(*it);
		}

	}

	public:

	~Graph() {
		for (std::vector<Node*>::iterator it = allNodes.begin(); it != allNodes.end(); it++) {
			delete *it;
		}
	}

	Graph() {
	//	num_nodes = 0;
		num_edges = 0;
		offset_cntr = 0;
	}
	
	inline Node* addNode() {
		
		/**
			add a new node to the graph
		*/
		
		Node* N = new Node;
		N->DependenciesCnt = 0;
		N->offset = offset_cntr;
		offset_cntr++;
		allNodes.push_back(N);
		
		return N;
	}

	inline void addDependency(Node* a, Node* b) {
		
		/** 
		   addDependency(a,b) means that a can not be
		   executed before b is finished 
		*/

		b->DependOnMe.push_back(a);
		a->DependenciesCnt++;
		num_edges++;
	}

	inline void addStartDependency(Node* a, Node *b) {
		
		/** 
		   addStartDependency(a,b) means that a can not be
		   executed before b is started
		*/

		b->StartDependOnMe.push_back(a);
		a->DependenciesCnt++;
		num_edges++;
	}

/*
	void serialize(FILE* fd, uint32_t rank, uint32_t num_ranks) {

		uint32_t buf32;

		rewind(fd);

		if (rank == 0) {
			// make room for jumptable
			fseek(fd, sizeof(uint64_t)*2*num_ranks + sizeof(uint32_t), SEEK_SET);
		}
		else {
			// jump to the end of the file
			fseek(fd, sizeof(uint32_t) + (2*(rank-1)+1)*sizeof(uint64_t), SEEK_SET);
			uint64_t eos;
			fread(&eos, sizeof(uint64_t), 1, fd);
			fseek(fd, eos, SEEK_SET);
		}
		
		find_root_nodes();
	
		long start = ftell(fd);
		// appendix starts right after all nodes
		long pos_in_appendix = start + 
		                       sizeof(uint32_t)*2 + // for Nodecount and Independent Actions count
							   sizeof(uint32_t)*RootNodes.size() + // for indp. actions offsets
							   (sizeof(char) + sizeof(uint8_t)*2 + sizeof(uint32_t)*7 + sizeof(uint64_t)) * allNodes.size(); // for actual nodeinfo
		// number of elements in the appendix so far
		uint32_t num_in_appendix = 0;

		// number of nodes in the schedule
		buf32 = (uint32_t) allNodes.size();
		fwrite( &buf32, sizeof(uint32_t), 1, fd );


		// write independant actions offsets
		buf32 = RootNodes.size(); 
		fwrite(&buf32, sizeof(uint32_t), 1, fd); // number of independent actions
		for (std::vector<Node*>::iterator it = RootNodes.begin(); it != RootNodes.end(); it++) {
			buf32 = (**it).offset;
			fwrite(&buf32, sizeof(uint32_t), 1, fd); // offset of independent action
		}
		

		Node n;
		int cnt = 0;
		for (std::vector<Node*>::iterator it = allNodes.begin(); it != allNodes.end(); it++) {
			
			cnt++;

			// write the actual operation info - this is a bit complicated thanks to padding that the compiler might do...
			fwrite( &((**it).DependenciesCnt), sizeof(uint32_t), 1, fd );
			fwrite( &((**it).Type), sizeof(char), 1, fd );
			fwrite( &((**it).Peer), sizeof(uint32_t), 1, fd );
			fwrite( &((**it).Size), sizeof(uint64_t), 1, fd );
			fwrite( &((**it).Tag), sizeof(uint32_t), 1, fd );
			fwrite( &((**it).Proc), sizeof(uint8_t), 1, fd );
			fwrite( &((**it).Nic), sizeof(uint8_t), 1, fd );
			
			// handling dependencies
			buf32 = (**it).DependOnMe.size();
			fwrite( &buf32, sizeof(uint32_t), 1, fd ); // number of ops depending on this node
			
			if ((**it).DependOnMe.size() > 0) {
				fwrite( &num_in_appendix, sizeof(uint32_t), 1, fd ); // start of dependant offsets in appendix
			} 
			else {
				uint32_t b = 999;
				fwrite( &b, sizeof(uint32_t), 1, fd ); // undefined
			}
			
			long pos = ftell(fd);
			// write offsets of depending ops into appendix
			fseek(fd, pos_in_appendix, SEEK_SET);
			for (std::vector<Node*>::iterator dit = (**it).DependOnMe.begin(); dit != (**it).DependOnMe.end(); dit++) {
				fwrite( &((**dit).offset), sizeof(uint32_t), 1, fd );
			}
			// adjust number of elements and position in appendix
			num_in_appendix += (**it).DependOnMe.size();
			pos_in_appendix = ftell(fd);
			// jump back to the node info (we're in appendix right now)
			fseek(fd, pos, SEEK_SET);
			
			// handling start-dependencies
			buf32 = (**it).StartDependOnMe.size();
			fwrite( &buf32, sizeof(uint32_t), 1, fd ); // number of ops start-depending on this node
			if ((**it).StartDependOnMe.size() > 0) { 
				fwrite( &num_in_appendix, sizeof(uint32_t), 1, fd ); // start of start-dependant offsets in appendix
			}
			else {
				uint32_t b = -1;
				fwrite( &b, sizeof(uint32_t), 1, fd ); // undefinedx
			}
			pos =  ftell(fd);
			// write offsets of start-depending ops into appendix
			fseek(fd, pos_in_appendix, SEEK_SET);
			for (std::vector<Node*>::iterator dit = (**it).StartDependOnMe.begin(); dit != (**it).StartDependOnMe.end(); dit++) {
				fwrite( &((**dit).offset), sizeof(uint32_t), 1, fd );
			}
			// adjust number of elements and position in appendix
			num_in_appendix += (**it).StartDependOnMe.size();
			pos_in_appendix = ftell(fd);
			// jump back to the node info (we're in appendix right now)
			fseek(fd, pos, SEEK_SET);
		
		}
		
		// write jumptable info: the schedule started at start and ended at pos_in_appendix
		fseek(fd, 0, SEEK_SET);
		
		// the number of ranks in this file
		buf32 = num_ranks;
		fwrite(&buf32, sizeof(uint32_t), 1, fd);
		fseek(fd, sizeof(uint64_t)*2*rank + sizeof(uint32_t), SEEK_SET);
		uint64_t buf64 = start;
		fwrite(&buf64, sizeof(uint64_t), 1, fd);
		buf64 = pos_in_appendix;
		fwrite(&buf64, sizeof(uint64_t), 1, fd);
	}

*/

	void serialize_mmap(int fd, uint32_t rank, uint32_t num_ranks, uint8_t max_cpu, uint8_t max_nic) {
		
		char *start_rankdata;
		uint64_t end_of_lastrank;
		static uint64_t filesize;
		char *pos;

		find_root_nodes();

		if (rank == 0) {
			// calculate the size of the file
			filesize = 0;
			filesize += sizeof(uint64_t); // magic cookie
			filesize += sizeof(uint32_t); // num ranks
			filesize += sizeof(uint8_t); // max_cpu
			filesize += sizeof(uint8_t); // max_nic
			filesize += sizeof(uint64_t)*2*num_ranks; // jumptable
			filesize += sizeof(uint32_t); // num nodes
			filesize += sizeof(uint32_t); // num indp actions
			filesize += (sizeof(uint32_t)*RootNodes.size()); // rootnodes offsets
			filesize += (sizeof(char)+sizeof(uint8_t)*2+sizeof(uint32_t)*7+sizeof(uint64_t))*allNodes.size(); // nodeinfo
			filesize += (sizeof(uint32_t)*num_edges); //appendix

			// enlarge the file
			lseek(fd, filesize-1, SEEK_SET);
			int r = write(fd, "", 1);
			assert(r == 1);
			
			// mmap the file
			mapping_start = (char*) mmap(NULL, filesize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
			if (mapping_start == MAP_FAILED) {
				perror("couldn't mmap the output file");
				exit(EXIT_FAILURE);
			}
			*( (uint64_t*) mapping_start ) = (uint64_t) MAGIC_COOKIE;
			mapping_start += sizeof(uint64_t); // jump over magic cookie
			end_of_lastrank = sizeof(uint32_t) + sizeof(uint8_t)*2 + sizeof(uint64_t)*2*num_ranks;
			start_rankdata = mapping_start + sizeof(uint32_t) + sizeof(uint8_t)*2 + sizeof(uint64_t)*2*num_ranks;  // our rankdata starts right after the jumptable
			
		}
		else {
			filesize += sizeof(uint32_t); // num nodes
			filesize += sizeof(uint32_t); // num indp actions
			filesize += sizeof(uint32_t)*RootNodes.size(); // rootnodes offsets
			filesize += (sizeof(char)+sizeof(uint8_t)*2+sizeof(uint32_t)*7+sizeof(uint64_t))*allNodes.size(); // nodeinfo
			filesize += sizeof(uint32_t)*num_edges; //appendix


			// enlarge the file
			lseek(fd, filesize-1, SEEK_SET);
			int r = write(fd, "", 1);
			assert(r == 1);

			// mmap the file
			mapping_start = (char*) mmap(NULL, filesize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
			if (mapping_start == MAP_FAILED) {
				perror("mmap failed");
				exit(EXIT_FAILURE);
			}

			mapping_start += sizeof(uint64_t); // jump over magic cookie

			end_of_lastrank = *((uint64_t*) (mapping_start + sizeof(uint32_t) + sizeof(uint8_t)*2 + sizeof(uint64_t)*(2*(rank-1)+1))); 
			start_rankdata = mapping_start + end_of_lastrank;
		}

		pos = start_rankdata;
		uint32_t num_in_appendix = 0;

		*((uint32_t*) pos) = (uint32_t) allNodes.size();	 	pos += sizeof(uint32_t);	// number of nodes in the schedule
		*((uint32_t*) pos) = (uint32_t) RootNodes.size();		pos += sizeof(uint32_t);	// number of independent actions
		
		// independent action offsets
		for (std::vector<Node*>::iterator it = RootNodes.begin(); it != RootNodes.end(); it++) {
			*((uint32_t*) pos) = (**it).offset; pos += sizeof(uint32_t);
		}

		// node data 

		for (std::vector<Node*>::iterator it = allNodes.begin(); it != allNodes.end(); it++) {

			*((uint32_t*) pos) = (**it).DependenciesCnt; 		pos += sizeof(uint32_t);	// number of actions this action depends on
			*((char*) pos)     = (**it).Type;            		pos += sizeof(char);		// type of this action
			*((uint32_t*) pos) = (**it).Peer;            		pos += sizeof(uint32_t);	// peer of this action
			*((uint64_t*) pos) = (**it).Size;            		pos += sizeof(uint64_t);	// size of the data transfer / length of the local calculation
			*((uint32_t*) pos) = (**it).Tag;             		pos += sizeof(uint32_t);	// tag of the send/recv operation
			*((uint8_t*) pos)  = (**it).Proc;            		pos += sizeof(uint8_t);		// processor used for this action
			*((uint8_t*) pos)  = (**it).Nic;             		pos += sizeof(uint8_t);		// network interface used for this action
			*((uint32_t*) pos) = (**it).DependOnMe.size();		pos += sizeof(uint32_t);	// number of actions that depend on this actions termination
			*((uint32_t*) pos) = num_in_appendix;				pos += sizeof(uint32_t);	// start index of dependent actions (in appendix)
			num_in_appendix += (**it).DependOnMe.size();
			*((uint32_t*) pos) = (**it).StartDependOnMe.size();	pos += sizeof(uint32_t);	// number of actions that depend on this actions start
			*((uint32_t*) pos) = num_in_appendix;				pos += sizeof(uint32_t);	// start index of start-dependent actions (in appendix)
			num_in_appendix += (**it).StartDependOnMe.size();
		}
		
		// appendix data
	
		for (std::vector<Node*>::iterator it = allNodes.begin(); it != allNodes.end(); it++) {
			for (std::vector<Node*>::iterator dit = (**it).DependOnMe.begin(); dit != (**it).DependOnMe.end(); dit++) {
				*((uint32_t*) pos) = (**dit).offset;       		pos += sizeof(uint32_t);	// offset of dependent action
			}
			for (std::vector<Node*>::iterator dit = (**it).StartDependOnMe.begin(); dit != (**it).StartDependOnMe.end(); dit++) {
				*((uint32_t*) pos) = (**dit).offset;       		pos += sizeof(uint32_t);	// offset of start-dependent action
			}
		}

		// jumptable info
		*((uint32_t*) mapping_start) = num_ranks;														// number of ranks in this schedule-file

		*((uint8_t*) (mapping_start + sizeof(uint32_t) )) = max_cpu;	// minimal number of cpu required to simulate this schedule
		*((uint8_t*) (mapping_start + sizeof(uint32_t) + sizeof(uint8_t) )) = max_nic;	// minimal number of nics required to simulate this schedule
		
		*((uint64_t*) (mapping_start + sizeof(uint32_t) + sizeof(uint8_t)*2 + sizeof(uint64_t)*2*rank)) = end_of_lastrank;	// start of this ranks info
		*((uint64_t*) (mapping_start + sizeof(uint32_t) + sizeof(uint8_t)*2 + sizeof(uint64_t)*(2*rank+1))) = pos - mapping_start;			// end of this ranks info
	
		//printf("s: %llu e: %llu\n", (long long unsigned int) end_of_lastrank, (long long unsigned int) (pos - mapping_start));

		// munmap the files so that the contents get written
		int r = munmap(mapping_start-sizeof(uint64_t), (size_t) (pos - mapping_start));
		assert(r == 0);	
	}

};

