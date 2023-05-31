/*
 File: vm_pool.C
 
 Author:
 Date  :
 
 */

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "machine.H"
#include "vm_pool.H"
#include "console.H"
#include "utils.H"
#include "assert.H"
#include "simple_keyboard.H"

/*--------------------------------------------------------------------------*/
/* DATA STRUCTURES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* CONSTANTS */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* FORWARDS */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* METHODS FOR CLASS   V M P o o l */
/*--------------------------------------------------------------------------*/

VMPool::VMPool(unsigned long  _base_address,
               unsigned long  _size,
               ContFramePool *_frame_pool,
               PageTable     *_page_table) {
    
    // Initialising variables
    this->_base_address = _base_address;
    this->_size = _size;
    this->_frame_pool = _frame_pool;
    this->_page_table = _page_table;
    this->region_iterator = 0;

    allocated_region = (struct allocated_vm_region*) (_base_address);
    this->_page_table->register_pool(this);

    Console::puts("Constructed VMPool object.\n");
}

unsigned long VMPool::allocate(unsigned long _size) {
    // _size cannot be zero.
    if(_size == 0) {
        Console::puts("Invalid size for allocate\n");
        assert(false);
        return 0;
    }
    // Virtual memory is full 
    if(region_iterator == MAX_VM_REGIONS) {
        Console::puts("VM full\n");
        assert(false);
        return 0;
    }

    //Total number of pages( n_pages_needed ) calculated
    unsigned int n_pages_needed = _size/Machine::PAGE_SIZE;
    if(_size % Machine::PAGE_SIZE > 0){
        n_pages_needed++;
    }

    // Calculating final_mem_size of the new segment along with _base_address for the segment
    unsigned long final_mem_size = n_pages_needed*Machine::PAGE_SIZE;
    if(region_iterator == 0){
        allocated_region[region_iterator]._base_address = _base_address;
    }
    else{
        allocated_region[region_iterator]._base_address = allocated_region[region_iterator-1]._base_address + allocated_region[region_iterator-1]._size;
    }

    // Setting _size variable to final_mem_size
    allocated_region[region_iterator]._size = final_mem_size;
    region_iterator++;

    Console::puts("Allocated region of memory.\n");
    return allocated_region[region_iterator-1]._base_address;
}

void VMPool::release(unsigned long _start_address) {
    // Finding indx for the memory pool
    unsigned int indx=0;
    for(;indx<region_iterator;indx++){
        if(allocated_region[indx]._base_address == _start_address){
            break;
        }
    }

    // Calculating number of pages alloted
    unsigned int n_pages_allocated = allocated_region[indx]._size/Machine::PAGE_SIZE;
    // Freeing the alloted pages
    for(unsigned int i=0;i<n_pages_allocated;i++){
        unsigned long address = allocated_region[indx]._base_address+i*Machine::PAGE_SIZE;
        _page_table->free_page(address);
    }

    // Updating the allocated_region array
    for(;indx<region_iterator;indx++){
        allocated_region[indx] = allocated_region[indx+1];
    }
    region_iterator--;

    // Flusing the TLB: We know that loading the page table also flushes the TLB
    _page_table->load();

    Console::puts("Released region of memory.\n");
}

bool VMPool::is_legitimate(unsigned long _address) {

    unsigned long last_address = _base_address + _size;
    if(_address>=_base_address && _address<=last_address)
        return true;
    return false;
    Console::puts("Checked whether address is part of an allocated region.\n");
}

