#include "DRAMAddr.hpp"




void DRAMAddr::set_base(void* buff) {
	base_msb = (size_t) buff & (~((size_t)(1ULL<<30) - 1)); // get higher order bits above the super page 
}


// TODO we can create a DRAMconfig class to load the right matrix depending on
// the configuration. You could also test it by checking if you can trigger bank conflcits
void DRAMAddr::load_mem_config(mem_config_t cfg) {
	MemConfig = Configs[cfg];

} 


DRAMAddr::DRAMAddr(size_t bk, size_t r, size_t c) {
	bank = bk;
	row = r;
	col = c;
}		

DRAMAddr::DRAMAddr(void* addr) {
	size_t p = (size_t) addr;
	size_t res = 0;
	for (size_t i = 0; i < MTX_SIZE; i++) {
		res <<= 1ULL;
		res |=  (size_t) __builtin_parityl(p & MemConfig.DRAM_MTX[i]);
	}
	bank 	= (res>>MemConfig.BK_SHIFT) & MemConfig.BK_MASK;
	row 	= (res>>MemConfig.ROW_SHIFT) & MemConfig.ROW_MASK;
	col 	= (res>>MemConfig.COL_SHIFT) & MemConfig.COL_MASK;
}		

size_t DRAMAddr::linearize() {
	return (this->bank << MemConfig.BK_SHIFT) | (this->row << MemConfig.ROW_SHIFT) | (this->col << MemConfig.COL_SHIFT);
}	



void* DRAMAddr::to_virt() {
	
	size_t res = 0;
	size_t l = this->linearize();
	for (size_t i = 0; i < MTX_SIZE; i++) {
		res <<= 1ULL;
		res |= (size_t) __builtin_parityl(l & MemConfig.ADDR_MTX[i]);
	}
	void* v_addr = (void*) (base_msb | res); 
	return v_addr;
}

std::string DRAMAddr::to_string() {
	char buff[1024];
	sprintf(buff, "DRAMAddr(b:%4ld, r:%10ld, c:%10ld) = %p", 
			this->bank,
	      		this->row,
	      		this->col,
	      		this->to_virt());
	return std::string(buff);
}

// Define the static DRAM configs

MemConfiguration DRAMAddr::MemConfig;
size_t DRAMAddr::base_msb;
std::map<size_t, MemConfiguration> DRAMAddr::Configs = {
	{(CHANS(1) | DIMMS(1) | RANKS(1) | BANKS(16)),
                {
               	BK_SHIFT        : 26,
                BK_MASK         : (0b1111),
                ROW_SHIFT       : 0,
                ROW_MASK        : (0b1111111111111),
                COL_SHIFT       : 13,
                COL_MASK        : (0b1111111111111),
                DRAM_MTX        : {
				0b000000000000000010000001000000,
				0b000000000000100100000000000000,
				0b000000000001001000000000000000,
				0b000000000010010000000000000000,
				0b000000000000000001000000000000,
				0b000000000000000000100000000000,
				0b000000000000000000010000000000,
				0b000000000000000000001000000000,
				0b000000000000000000000100000000,
				0b000000000000000000000010000000,
				0b000000000000000000000001000000,
				0b000000000000000000000000100000,
				0b000000000000000000000000010000,
				0b000000000000000000000000001000,
				0b000000000000000000000000000100,
				0b000000000000000000000000000010,
				0b000000000000000000000000000001,
				0b100000000000000000000000000000,
				0b010000000000000000000000000000,
				0b001000000000000000000000000000,
				0b000100000000000000000000000000,
				0b000010000000000000000000000000,
				0b000001000000000000000000000000,
				0b000000100000000000000000000000,
				0b000000010000000000000000000000,
				0b000000001000000000000000000000,
				0b000000000100000000000000000000,
				0b000000000010000000000000000000,
				0b000000000001000000000000000000,
				0b000000000000100000000000000000
		},
		ADDR_MTX	: {
				0b000000000000000001000000000000,
				0b000000000000000000100000000000,
				0b000000000000000000010000000000,
				0b000000000000000000001000000000,
				0b000000000000000000000100000000,
				0b000000000000000000000010000000,
				0b000000000000000000000001000000,
				0b000000000000000000000000100000,
				0b000000000000000000000000010000,
				0b000000000000000000000000001000,
				0b000000000000000000000000000100,
				0b000000000000000000000000000010,
				0b000000000000000000000000000001,
				0b000100000000000000000000000100,
				0b001000000000000000000000000010,
				0b010000000000000000000000000001,
				0b100000000010000000000000000000,
				0b000010000000000000000000000000,
				0b000001000000000000000000000000,
				0b000000100000000000000000000000,
				0b000000010000000000000000000000,
				0b000000001000000000000000000000,
				0b000000000100000000000000000000,
				0b000000000010000000000000000000,
				0b000000000001000000000000000000,
				0b000000000000100000000000000000,
				0b000000000000010000000000000000,
				0b000000000000001000000000000000,
				0b000000000000000100000000000000,
				0b000000000000000010000000000000
				}
		}
	},
	{(CHANS(1) | DIMMS(1) | RANKS(2) | BANKS(16)), 
		{ 
		BK_SHIFT	: 25,
		BK_MASK      	: (0b11111),
		ROW_SHIFT	: 0,
		ROW_MASK     	: (0b111111111111),
		COL_SHIFT    	: 12,
		COL_MASK     	: (0b1111111111111),
		DRAM_MTX	: {
				0b000000000000000010000001000000,
				0b000000000001000100000000000000,
				0b000000000010001000000000000000,
				0b000000000100010000000000000000,
				0b000000001000100000000000000000,
				0b000000000000000001000000000000,
				0b000000000000000000100000000000,
				0b000000000000000000010000000000,
				0b000000000000000000001000000000,
				0b000000000000000000000100000000,
				0b000000000000000000000010000000,
				0b000000000000000000000001000000,
				0b000000000000000000000000100000,
				0b000000000000000000000000010000,
				0b000000000000000000000000001000,
				0b000000000000000000000000000100,
				0b000000000000000000000000000010,
				0b000000000000000000000000000001,
				0b100000000000000000000000000000,
				0b010000000000000000000000000000,
				0b001000000000000000000000000000,
				0b000100000000000000000000000000,
				0b000010000000000000000000000000,
				0b000001000000000000000000000000,
				0b000000100000000000000000000000,
				0b000000010000000000000000000000,
				0b000000001000000000000000000000,
				0b000000000100000000000000000000,
				0b000000000010000000000000000000,
				0b000000000001000000000000000000	
				},
		ADDR_MTX 	: {
			        0b000000000000000000100000000000,
				0b000000000000000000010000000000,
				0b000000000000000000001000000000,
				0b000000000000000000000100000000,
				0b000000000000000000000010000000,
				0b000000000000000000000001000000,
				0b000000000000000000000000100000,
				0b000000000000000000000000010000,
				0b000000000000000000000000001000,
				0b000000000000000000000000000100,
				0b000000000000000000000000000010,
				0b000000000000000000000000000001,
				0b000010000000000000000000001000,
				0b000100000000000000000000000100,
				0b001000000000000000000000000010,
				0b010000000000000000000000000001,
				0b100000000001000000000000000000,
				0b000001000000000000000000000000,
				0b000000100000000000000000000000,
				0b000000010000000000000000000000,
				0b000000001000000000000000000000,
				0b000000000100000000000000000000,
				0b000000000010000000000000000000,
				0b000000000001000000000000000000,
				0b000000000000100000000000000000,
				0b000000000000010000000000000000,
				0b000000000000001000000000000000,
				0b000000000000000100000000000000,
				0b000000000000000010000000000000,
				0b000000000000000001000000000000	
				}
		}	
	}
};

