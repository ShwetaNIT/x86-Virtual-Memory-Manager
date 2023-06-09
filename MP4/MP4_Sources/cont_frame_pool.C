/*
 File: ContFramePool.C
 
 Author:
 Date  : 
 
 */

/*--------------------------------------------------------------------------*/
/* 
 POSSIBLE IMPLEMENTATION
 -----------------------

 The class SimpleFramePool in file "simple_frame_pool.H/C" describes an
 incomplete vanilla implementation of a frame pool that allocates 
 *single* frames at a time. Because it does allocate one frame at a time, 
 it does not guarantee that a sequence of frames is allocated contiguously.
 This can cause problems.
 
 The class ContFramePool has the ability to allocate either single frames,
 or sequences of contiguous frames. This affects how we manage the
 free frames. In SimpleFramePool it is sufficient to maintain the free 
 frames.
 In ContFramePool we need to maintain free *sequences* of frames.
 
 This can be done in many ways, ranging from extensions to bitmaps to 
 free-lists of frames etc.
 
 IMPLEMENTATION:
 
 One simple way to manage sequences of free frames is to add a minor
 extension to the bitmap idea of SimpleFramePool: Instead of maintaining
 whether a frame is FREE or ALLOCATED, which requires one bit per frame, 
 we maintain whether the frame is FREE, or ALLOCATED, or HEAD-OF-SEQUENCE.
 The meaning of FREE is the same as in SimpleFramePool. 
 If a frame is marked as HEAD-OF-SEQUENCE, this means that it is allocated
 and that it is the first such frame in a sequence of frames. Allocated
 frames that are not first in a sequence are marked as ALLOCATED.
 
 NOTE: If we use this scheme to allocate only single frames, then all 
 frames are marked as either FREE or HEAD-OF-SEQUENCE.
 
 NOTE: In SimpleFramePool we needed only one bit to store the state of 
 each frame. Now we need two bits. In a first implementation you can choose
 to use one char per frame. This will allow you to check for a given status
 without having to do bit manipulations. Once you get this to work, 
 revisit the implementation and change it to using two bits. You will get 
 an efficiency penalty if you use one char (i.e., 8 bits) per frame when
 two bits do the trick.
 
 DETAILED IMPLEMENTATION:
 
 How can we use the HEAD-OF-SEQUENCE state to implement a contiguous
 allocator? Let's look a the individual functions:
 
 Constructor: Initialize all frames to FREE, except for any frames that you 
 need for the management of the frame pool, if any.
 
 get_frames(_n_frames): Traverse the "bitmap" of states and look for a 
 sequence of at least _n_frames entries that are FREE. If you find one, 
 mark the first one as HEAD-OF-SEQUENCE and the remaining _n_frames-1 as
 ALLOCATED.

 release_frames(_first_frame_no): Check whether the first frame is marked as
 HEAD-OF-SEQUENCE. If not, something went wrong. If it is, mark it as FREE.
 Traverse the subsequent frames until you reach one that is FREE or 
 HEAD-OF-SEQUENCE. Until then, mark the frames that you traverse as FREE.
 
 mark_inaccessible(_base_frame_no, _n_frames): This is no different than
 get_frames, without having to search for the free sequence. You tell the
 allocator exactly which frame to mark as HEAD-OF-SEQUENCE and how many
 frames after that to mark as ALLOCATED.
 
 needed_info_frames(_n_frames): This depends on how many bits you need 
 to store the state of each frame. If you use a char to represent the state
 of a frame, then you need one info frame for each FRAME_SIZE frames.
 
 A WORD ABOUT RELEASE_FRAMES():
 
 When we releae a frame, we only know its frame number. At the time
 of a frame's release, we don't know necessarily which pool it came
 from. Therefore, the function "release_frame" is static, i.e., 
 not associated with a particular frame pool.
 
 This problem is related to the lack of a so-called "placement delete" in
 C++. For a discussion of this see Stroustrup's FAQ:
 http://www.stroustrup.com/bs_faq2.html#placement-delete
 
 */
/*--------------------------------------------------------------------------*/


/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/
#define KB * (0x1 << 10)
/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "cont_frame_pool.H"
#include "console.H"
#include "utils.H"
#include "assert.H"

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
/* METHODS FOR CLASS   C o n t F r a m e P o o l */
/*--------------------------------------------------------------------------*/
ContFramePool::FrameState ContFramePool::get_state(unsigned long _frame_no) {
    unsigned int bitmap_index = _frame_no / 4;
    unsigned long position = 2*(_frame_no % 4);
    unsigned char mask = 0x1 << (position) | 0x1 << (position+1);
    unsigned long result = bitmap[bitmap_index] & mask;
    if (result==0)
        return FrameState::Free;
    else if (result==mask)
        return FrameState::Used;
    return FrameState::HoS;
}

void ContFramePool::set_state(unsigned long _frame_no, FrameState _state) {
    unsigned int bitmap_index = _frame_no / 4;
    unsigned long position = 2*(_frame_no % 4);
    unsigned char mask = 0x1 << (position) | 0x1 << (position+1);

    switch(_state) {
      case FrameState::Used: // axyb , 0110 -> a11b (11)
        bitmap[bitmap_index] |= mask;
        break;
      case FrameState::Free: // axyb 0110 -> a00b (00)
        bitmap[bitmap_index] &= ~mask;
        break;
      case FrameState::HoS: // axyb 0110 -> a10b (10)
        bitmap[bitmap_index] &= ~mask;
        mask = 0x1 << (position+1);
        bitmap[bitmap_index] |= mask;
        break;
    }
    
}

ContFramePool* ContFramePool::list_head;
ContFramePool* ContFramePool::last_node;

ContFramePool::ContFramePool(unsigned long _base_frame_no,
                             unsigned long _n_frames,
                             unsigned long _info_frame_no)
{
    // Bitmap must fit in a single frame!
    assert(_n_frames <= FRAME_SIZE * 8);
    
    base_frame_no = _base_frame_no;
    n_frames = _n_frames;
    n_free_frames = _n_frames;
    info_frame_no = _info_frame_no;
    
    // If _info_frame_no is zero then we keep management info in the first
    //frame, else we use the provided frame to keep management info
    if(info_frame_no == 0) {
        bitmap = (unsigned char *) (base_frame_no * FRAME_SIZE);
    } else {
        bitmap = (unsigned char *) (info_frame_no * FRAME_SIZE);
    }
    
    // Everything ok. Proceed to mark all frame as free.
    for(unsigned long fno = 0; fno < n_frames; fno++) {
        set_state(fno, FrameState::Free);
    }
    
    // Mark the first frame as being used if it is being used
    if(info_frame_no == 0) {
        set_state(0, FrameState::Used);
        n_free_frames--;
    }
    
    //Adding the current frame pool to pool list for better release handling.
    if(ContFramePool::list_head==NULL){
        ContFramePool::list_head = this;
        ContFramePool::last_node = this;
    } else {
        ContFramePool::last_node -> next_pool = this; 
        ContFramePool::last_node = this;
    }
    next_pool=NULL;

    Console::puts("Frame Pool initialized\n");
}

unsigned long ContFramePool::get_frames(unsigned int _n_frames)
{
    // Any frames left to allocate?
    assert(n_free_frames > 0);
    
    // Find a frame that is not being used and return its frame index.
    // Mark that frame as being used in the bitmap.
    unsigned long i=0, found=0, count=0;
    for(;i<n_frames;i++){
        //if(bitmap[i] == 'f')
        if(get_state(i) == FrameState::Free)
            count++;
        else
            count = 0;
        if(count==_n_frames) {
            found=1;
            break;
        }
    }
       
    if(found==0){
        Console::puts("Continuous memory not found\n");
        return 0;
    }
    unsigned long start_frame = i-_n_frames+1;
    set_state(start_frame, FrameState::HoS);
    n_free_frames--;
    for(i=start_frame+1;i<start_frame+_n_frames;i++){
    set_state(i, FrameState::Used);
    n_free_frames--;
    }
    return (start_frame + base_frame_no);
}

void ContFramePool::mark_inaccessible(unsigned long _base_frame_no,
                                      unsigned long _n_frames)
{
    set_state(_base_frame_no, FrameState::HoS);
    n_free_frames--;
    
    for(unsigned long i = _base_frame_no+1; i<_base_frame_no+_n_frames; i++){
        set_state(i, FrameState::Used);
        n_free_frames--;
    }
    
}

void ContFramePool::release_frames(unsigned long _first_frame_no)
{
    ContFramePool* curr_pool = ContFramePool::list_head;   
    while(curr_pool != NULL) {
        if((curr_pool->base_frame_no<=_first_frame_no) && (_first_frame_no < (curr_pool->base_frame_no + curr_pool->n_frames))) {
            break;
        }
        curr_pool = curr_pool -> next_pool;
    }
    
    if(curr_pool == NULL) {
        Console::puts("Pool not found\n");
        return;
    }
    
    unsigned char * curr_pool_bitmap = curr_pool->bitmap;
    if(curr_pool->get_state(_first_frame_no-curr_pool->base_frame_no) != FrameState::HoS) {
        Console::puts("First frame is not a head frame\n");
        return;
    }
    
    curr_pool->set_state(_first_frame_no-curr_pool->base_frame_no, FrameState::Free);
    curr_pool->n_free_frames++;
    _first_frame_no++;

    while(curr_pool->get_state(_first_frame_no-curr_pool->base_frame_no) == FrameState::Used) {
        curr_pool->set_state(_first_frame_no-curr_pool->base_frame_no, FrameState::Free);
        curr_pool->n_free_frames++;
        _first_frame_no++;
    }
}

unsigned long ContFramePool::needed_info_frames(unsigned long _n_frames)
{
    unsigned long max_bits_in_frame = 8*ContFramePool::FRAME_SIZE/2;
    return _n_frames / max_bits_in_frame + (_n_frames % max_bits_in_frame > 0 ? 1 : 0);
}