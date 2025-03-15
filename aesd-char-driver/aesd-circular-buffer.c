/**
 * @file aesd-circular-buffer.c
 * @brief Functions and data related to a circular buffer imlementation
 *
 * @author Dan Walkes
 * @date 2020-03-01
 * @copyright Copyright (c) 2020
 *
 *@references https://github.com/CU-ECEN-5823/ecen5823-assignment05-aysvarya-gopinath/blob/master/circular_buffer.c (implemented a similar logic)
 */

 #ifdef __KERNEL__
 #include <linux/string.h>
 #else
 #include <string.h>
 #endif
 #include "stdio.h"
 #include "aesd-circular-buffer.h"
 
 /*Function to calculate the next pointer position
 *Take an ins_offs or out_offs as inuput parameter
 *Returns a pointer 
 */
  uint8_t nextPtr(uint8_t ptr) {
   ptr++;
  if (ptr == AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED) // if the next pointer is equal to buffer max depth
  return 0; // wrap back to 0
  else
  return ptr; // advance
 } 
 
 /**
 * Initializes the circular buffer described by @param buffer to an empty struct
 */
 void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
 {
	 memset(buffer,0,sizeof(struct aesd_circular_buffer));
 }
 
 /**
  * @param buffer the buffer to search for corresponding offset.  Any necessary locking must be performed by caller.
  * @param char_offset the position to search for in the buffer list, describing the zero referenced
  *      character index if all buffer strings were concatenated end to end
  * @param entry_offset_byte_rtn is a pointer specifying a location to store the byte of the returned aesd_buffer_entry
  *      buffptr member corresponding to char_offset.  This value is only set when a matching char_offset is found
  *      in aesd_buffer.
  * @return the struct aesd_buffer_entry structure representing the position described by char_offset, or
  * NULL if this position is not available in the buffer (not enough data is written).
  */
struct aesd_buffer_entry *aesd_circular_buffer_find_entry_offset_for_fpos(
    struct aesd_circular_buffer *buffer, size_t char_offset, size_t *entry_offset_byte_rtn)
{
    uint8_t index = buffer->out_offs;  // Start from the oldest entry
    struct aesd_buffer_entry *entryptr;
    size_t byte_offset, chars_traversed = 0;  // Tracks the number of characters currently traversed

   while (1) {   //for loop didnt hanlde boundaries
        entryptr = &buffer->entry[index];  // Get the current entry    
        if (char_offset < chars_traversed + entryptr->size) { //
           byte_offset = char_offset - chars_traversed ;
            *entry_offset_byte_rtn = byte_offset;  // Set the byte offset
            return entryptr;
        } 
       chars_traversed +=entryptr->size;
        index=nextPtr(index);
        if (index == buffer->in_offs) 
            break;     
    }
    return NULL; 
}

 
 /**
 * Adds entry @param add_entry to @param buffer in the location specified in buffer->in_offs.
 * If the buffer was already full, overwrites the oldest entry and advances buffer->out_offs to the
 * new start location.
 * Any necessary locking must be handled by the caller
 * Any memory referenced in @param add_entry must be allocated by and/or must have a lifetime managed by the caller.
 *Returns the pointer to the replaced entry to be freed when buffer is full and an entry is overwritten else NULL .
 */
const char *aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer, const struct aesd_buffer_entry *add_entry)
{
 const char *buff_return = NULL; //returns the pointer to the ent before overwritting 
    if((buffer->in_offs == buffer->out_offs)&&(buffer->full ==true))  //buffer is full so overwrite the old value
   {
                   buff_return = buffer->entry[buffer->in_offs].buffptr; //return the pointer to be overwritten for freeing
		   buffer->entry[buffer->in_offs]= *add_entry ; //overwrite the oldest value
	           buffer->out_offs =nextPtr(buffer->out_offs); //increment both pointers
		   buffer->in_offs =nextPtr(buffer->in_offs) ; 
   } 
   else  //buffer is empty so add entries
   {
	   buffer->entry[buffer->in_offs]= *add_entry ;//add element structure at the input pointer
	   buffer->in_offs =nextPtr(buffer->in_offs) ; //increment the input pointer		  
   }
   buffer->full =(buffer->in_offs == buffer->out_offs); //check if buffer is full
   return (buff_return);
}

 
  

 
 
