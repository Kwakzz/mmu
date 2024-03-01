#include<stdio.h>
#include<stdlib.h>
#include<time.h>
#include<math.h>


// CONSTANTS
#define MAX_PROCESS_COUNT 8 // Max process count is the size of the outer page table.

// Let's assume all integers related to size are in bytes
#define MIN_PROCESS_SIZE 16  // 16B 
#define MAX_PROCESS_SIZE 64 // 64B  
// A process is allowed to have a maximum of 4 pages. A page is 16 bytes and the max process size is 64 bytes.


// 2^n=frame size; n = 2, 2 bits required to uniquely represent a frame of 4 bytes
#define FRAME_SIZE 16
// e.g., in a 1024 byte physical memory, the addresses will be m bits (where m=10) since 2^10=1024. 10 bits are required to uniquely represent addresses in a 1024-byte physical memory. So an address for each byte. That's how a byte-addressable memory system works.
#define PHYSICAL_MEMORY_SIZE (FRAME_SIZE * 64) // 1024 bytes; 64 frames
#define NO_OF_FRAMES (PHYSICAL_MEMORY_SIZE/FRAME_SIZE)
// for a physical memory of 1024 bytes and frames of 16 bytes,
// m = 10, n = 4
// 10-bit address
// 6 bits for frame number,
// 4 bits for offset

#define PAGE_SIZE FRAME_SIZE // pages and frames have the same size
#define VIRTUAL_MEMORY_SIZE (4096) // 4096 bytes
// 12 bits required to uniquely represent addresses in a 4096-byte memory. 2^12=4096
#define NO_OF_PAGES (VIRTUAL_MEMORY_SIZE / PAGE_SIZE) // 4096/16 = 256 pages
// n = 4, m = 12
// 12-bit address
// 8 bits for page number,
// 4 bits for offset

#define PAGE_TABLE_ENTRY_SIZE 4 // 4 bytes
// How many page table entries can fit in a page? 16bytes/4bytes = 4 page entries in a page
// 2^2 = 4. 2-bit address for offset within page number. Original page number is 8 bits
// Page number within page number = 8-2 = 6 bits long
// p1 (6 bits of page number within page number) maps on to outer page table
// p2 (2 bits of offset within page number) displacement within page of inner table
#define NO_OF_PAGE_TABLE_ENTRIES_IN_PAGE (PAGE_SIZE/PAGE_TABLE_ENTRY_SIZE)  // 4
#define OUTER_PAGE_TABLE_SIZE (NO_OF_PAGES/NO_OF_PAGE_TABLE_ENTRIES_IN_PAGE) // 256/4 = 64
// OUTER PAGE TABLE 64x4 (64 blocks/pages, 4 page entries within each block)


/**
 * @brief A struct representing a page table entry.
 * The page table array is of type page_table_entry.
 * @param frame_number - Of type int. The number of the frame the page being represented maps on to.
 * @param valid - Of type boolean. Indicates whether a page has a corresponding frame
 */
struct page_table_entry
{
    int frame_number;
    int valid;
};

/**
 * @brief A struct representing a process control block (PCB). The PCB contains info about a process. For this program we'll focus on id, size and outer page table (Each process has its own page table) (And this program uses hierarchical paging)
 * @param id - The id of the process. Generated sequentially.
 * @param size - The size of the process in bytes. Determines the number of frames it will require. For example, a process of size 12 bytes in an MMU using pages of size 4 bytes will require 3 pages. If a process requires 3 pages, it requires n frames since frame size is equal to frame size.
 * @param size_in_memory The amount of space in memory the process is occupying. It's an int value.
 * @param inner_page_tables A 2D array representing the inner page tables of a process.
 */
struct PCB {
    int id;
    int size;
    int size_in_memory;
    struct page_table_entry inner_page_tables[OUTER_PAGE_TABLE_SIZE][NO_OF_PAGE_TABLE_ENTRIES_IN_PAGE];
};


/**
 * @brief 
 * @param page_number The number of the page
 * @param process The process currently holding the page
 */
struct page
{
    int page_number;
    struct PCB process;
};



// Row value represents number of frames or pages. Column value represents number of bytes within a frame or page.
struct PCB physical_memory [NO_OF_FRAMES][FRAME_SIZE]; // [64][16]
struct page virtual_memory[NO_OF_PAGES][PAGE_SIZE]; // [256]
struct PCB* master_outer_page_table[MAX_PROCESS_COUNT];
int physical_memory_remaining = PHYSICAL_MEMORY_SIZE;
int no_of_page_faults = 0;

// FUNCTION DECLARATIONS
int generate_random_logical_address();
int generate_random_process_size();
int generate_random_request_size(int process_size);

struct PCB create_process(int process_number);
int memory_request(struct PCB *process);
int find_process_page_number (struct PCB *process);
int find_process_frame_number (struct PCB *process); 

void initialize_process_page_tables(struct PCB *process);
void hierarchical_translation(int logical_address, struct PCB *process);
int first_fit(struct PCB *process, int offset);
void update_page_table(struct PCB *process, int logical_address, int frame_number);
void memory_deallocation(struct PCB *process);

void initialize_physical_memory();
void initialize_virtual_memory();
void visualize_physical_memory();
void visualize_virtual_memory();
void visualize_inner_page_tables(struct PCB *process);



// --- MAIN ---
int main () {

    // Seed the random number generator with the current time
    srand((unsigned int)time(NULL));

    
    printf("This is a simulation of an MMU. In this simulation, -1 indicates an empty memory address\n");
    printf("Below are its specs:\n\n");

    // Display physical memory specs
    printf("PHYSICAL MEMORY SIZE: %d bytes\n", PHYSICAL_MEMORY_SIZE);
    printf("NO OF FRAMES IN PHYSICAL MEMORY: %d\n", NO_OF_FRAMES);
    printf("FRAME SIZE: %d bytes\n\n", FRAME_SIZE);

    // Display virtual memory specs
    printf("VIRTUAL MEMORY SIZE: %d bytes\n", VIRTUAL_MEMORY_SIZE);
    printf("NO OF PAGES IN VIRTUAL MEMORY: %d\n\n", NO_OF_PAGES);
    printf("PAGE SIZE: %d bytes\n\n", PAGE_SIZE);

    // initialize physical memory
    printf("Initializing physical memory...\n");
    initialize_physical_memory();

    // initialize virtual memory
    printf("Initializing virtual memory...\n");
    initialize_virtual_memory();

    int num_of_processes;

    printf("\nHow many processes do you want to create? Maximum is 64\n");
    scanf("%d", &num_of_processes);

    printf("Creating processes...\n\n");

    for (int i = 0; i < num_of_processes; i++) {
        
        // create process
        struct PCB process = create_process(i);

        // if process wasn't created, end simulation
        if (process.id == -1) {
            exit(0);
        }

        initialize_process_page_tables(&process);

        int memory_request_size = memory_request(&process);

        if (memory_request_size != -1) {
            // generate random logical address. This logical address points to a page which will be assigned to the process.
            int logical_address = generate_random_logical_address();

            // allocate memory to processes
            hierarchical_translation(logical_address, &process);
            visualize_inner_page_tables(&process);
        }

    }

    // visualize_physical_memory();
    visualize_physical_memory();
    visualize_virtual_memory();

    for (int i =0; i<num_of_processes; i++) {
        memory_deallocation(master_outer_page_table[i]);
    }


    visualize_physical_memory();
    visualize_virtual_memory();


    // Free allocated memory for each process when done
    for (int i = 0; i < MAX_PROCESS_COUNT; i++) {
        free(master_outer_page_table[i]);
    }

 

    return 0;
}






// --- FUNCTIONS --- 


/**
 * @brief This function translates a logical address to a physical address using two-level paging. The page number and offset are obtained from the address. Then, the inner page table number and offset are obtained from the page number. The inner page table number  determines the inner page table which contains the page table entry of the page with the logical address. The inner page table offset then directs you to the specific page. Each process has an inner page tables 2D array. The rows represent inner page tables, and the entries are page table entries.
 * 
 * @param logical_address The logical address to be translated to a physical address. This logical address is randomly generated.
 */
void hierarchical_translation(int logical_address, struct PCB *process) {

    printf("Logical Address Generated by Process %d is %d\n", process->id, logical_address);

    int page_number = logical_address / PAGE_SIZE;
    int offset = logical_address % PAGE_SIZE;

    printf("Page Number: %d\n", page_number);
    printf("Offset: %d\n", offset);

    // assign process to page(s) in virtual memory.
    int required_no_of_pages = (int)ceil((double)process->size_in_memory / PAGE_SIZE );
    for (int i = page_number; i < page_number + required_no_of_pages; i++) {
        for (int j = 0; j < FRAME_SIZE; j++) {
            virtual_memory[i][offset].process = *process;
        }
    }

    int inner_page_table_no = page_number / NO_OF_PAGE_TABLE_ENTRIES_IN_PAGE;
    int inner_page_table_offset = page_number % NO_OF_PAGE_TABLE_ENTRIES_IN_PAGE;

    struct page_table_entry pte = process->inner_page_tables[inner_page_table_no][inner_page_table_offset];

    if (pte.frame_number == -1) {

        no_of_page_faults++;
        printf("Page Fault (Page entry has not yet been assigned a frame).\n");

        // find contiguous block of frames for process
        int frame_number = first_fit(process, offset);

        // update page table after memory allocation
        update_page_table(process, logical_address, frame_number);
    }

    else {
       printf("Page %d has already been assigned to a frame for this process", page_number);
    }
}


/**
 * @brief This function generates a random 12-bit logical address but in decimal. It's 12 bits because the size of the virtual memory is 2^32.
 * 
 * @return A randomly generated decimal not greater than the VIRTUAL MEMORY SIZE
 */
int generate_random_logical_address() {
    return rand() % VIRTUAL_MEMORY_SIZE;
}



/**
 * @brief generate a random size for a process
 * 
 * @return int the randomly generated process size.
 */
int generate_random_process_size() {
    return (rand() % (MAX_PROCESS_SIZE - MIN_PROCESS_SIZE + 1)) + MIN_PROCESS_SIZE;
}

/**
 * @brief Generate a random memory capacity as a process' request for memory.
 * 
 * @param process_size the size of the process requesting for memory. A process cannot request for memory space greater than its size. The memory requested is at least 1 byte.
 * @return A random memory request size of type int.
 */
int generate_random_request_size(int process_size) {
    return (rand() % (process_size - 1 + 1)) + 1;
}


/**
 * @brief First fit algorithm for memory allocation
 * 
 * @param process The process to be allocated memory
 * @param offset The number of bytes in a frame a process needs to occupy.
 * 
 * @return The starting frame number for the process. This frame number is used to update the page table.
 */
int first_fit(struct PCB *process, int offset) {
    printf("Using first fit algorithm to allocate memory for Process %d, with size %d bytes...\n", process->id, process->size_in_memory);

    int required_no_of_frames = (int)ceil((double)process->size_in_memory / FRAME_SIZE);
    printf("Process with ID, %d, requires %d frames\n", process->id, required_no_of_frames);

    int frame_counter = 0;
    int start_frame = -1;

    // Iterate through physical memory to find the first block of consecutive free frames
    for (int i = 0; i < NO_OF_FRAMES; i++) {
        int byte_counter = 0;
        while (byte_counter != FRAME_SIZE) {
            if (physical_memory[i][byte_counter].id != -1) {
                start_frame = -1;
                frame_counter = 0;
                break;
            }
            byte_counter++;
        }

        if (byte_counter == FRAME_SIZE) {
            if (start_frame == -1) {
                start_frame = i;
            }
            frame_counter++;
        }

        if (frame_counter == required_no_of_frames) {
            break;  // Found a suitable block
        }
    }

    // Check if a suitable block was found
    if (frame_counter == required_no_of_frames) {
        // Update physical memory to store the process identifier or reference
        for (int i = start_frame; i < start_frame + required_no_of_frames; i++) {
            for (int j = 0; j < FRAME_SIZE; j++) {
                physical_memory[i][offset] = *process;
            }
        }

        printf("Memory allocated successfully at frame %d for process with ID %d. Process is occupying %d frames.\n", start_frame, 
        process->id, required_no_of_frames);
        physical_memory_remaining -= process->size;
        printf("%d bytes of physical memory remaining.\n\n", physical_memory_remaining);
        return start_frame;
    }

    printf("No free frame was found for process with id %d\n", process->id);
    return -1;
}



/**
 * @brief This function initializes the inner page tables of a process. It sets every frame number of every page table entry to -1, and the valid bit to 0. This is to show that no page table entry has been assigned a frame yet (since at this point, the OS hasn't generated a logical address for it, and thus, it has not yet requested memory).
 * 
 * @param process The process whose page table is to be initialized.
 */
void initialize_process_page_tables(struct PCB *process) {

    // Initialize inner page tables
    for (int i = 0; i < OUTER_PAGE_TABLE_SIZE; i++) {
        for (int j = 0; j < NO_OF_PAGE_TABLE_ENTRIES_IN_PAGE; j++) {
            process->inner_page_tables[i][j].frame_number = -1;
            process->inner_page_tables[i][j].valid = 0;
        }
    }
}


/**
 * @brief Update the process's page table. This is done after allocating it memory.
 * 
 * @param process The process whose page table is to be updated.
 * @param logical_address The logical address generated when the process was recently stored in physical memory.
 * @param frame_number The starting frame number of the process that was stored in memory.
 */
void update_page_table(struct PCB *process, int logical_address, int frame_number) {
    int page_number = logical_address / PAGE_SIZE;
    int offset = logical_address % PAGE_SIZE;

    int inner_page_table_no = page_number / NO_OF_PAGE_TABLE_ENTRIES_IN_PAGE;
    int inner_page_table_offset = page_number % NO_OF_PAGE_TABLE_ENTRIES_IN_PAGE;

    process->inner_page_tables[inner_page_table_no][inner_page_table_offset].frame_number = frame_number;
    process->inner_page_tables[inner_page_table_no][inner_page_table_offset].valid = 1;

    printf("Process %d has been assigned to page %d and offset %d.\n\n", process->id, page_number, offset);
}


/**
 * @brief Initialize virtual memory by setting every page's number to its corresponding row index in the 2D array. Note that virtual memory is a 2D array. The array virtual_memory has been declared globally.
 */
void initialize_virtual_memory() {

    for (int i = 0; i < NO_OF_PAGES; i++) {
        for (int j = 0; j < PAGE_SIZE; j++) {
            virtual_memory[i][j].page_number = i;
            virtual_memory[i][j].process.id = -1;
        }
    }

    printf("Virtual memory initialized.\n\n");

}


/**
 * @brief Initialize main memory by setting every frame cell's process's id to -1. -1 means it's empty. Note that physical memory is a 2D array. The array physical_memory has been declared globally.
 */
void initialize_physical_memory() {
    // Assuming physical_memory is already declared globally or in the function scope

    for (int i = 0; i < NO_OF_FRAMES; i++) {
        for (int j = 0; j < FRAME_SIZE; j++) {
            physical_memory[i][j].id = -1;
        }
    }

    printf("Physical memory initialized.\n\n");


}


/**
 * @brief Visualize physical memory in a tabular format
 * 
 */
void visualize_physical_memory() {
    printf("Physical Memory Visualization:\n");

    for (int i = 0; i < NO_OF_FRAMES; i++) {
        for (int j = 0; j < FRAME_SIZE; j++) {
            if (physical_memory[i][j].id != -1) {
                printf("%2d ", physical_memory[i][j].id);
            } else {
                printf(" - ");
            }
        }
        printf("\n");
    }
}



/**
 * @brief Visualize virtual memory in a tabular format
 * 
 */
void visualize_virtual_memory() {
    printf("Virtual Memory Visualization:\n");

    for (int i = 0; i < NO_OF_PAGES; i++) {
        for (int j = 0; j < PAGE_SIZE; j++) {
            if (virtual_memory[i][j].process.id != -1) {
                printf("%2d ", virtual_memory[i][j].process.id);
            } else {
                printf(" - ");
            }
        }
        printf("\n");
    }
}


/**
 * @brief Create a process object
 * 
 * @param process_number The index of the process in the master outer page table. This is also equal to the process' ID. 
 * @return If process creation is successful, return an object of type struct PCB with a size, and an ID >= 0. Otherwise, return a process with an ID of -1.
 */
struct PCB create_process(int process_number) {

    size_t no_of_processes = sizeof(master_outer_page_table) / sizeof(master_outer_page_table[0]);

    // check if the maximum process count has been reached. If it has, don't allow the process to be created.
    if (no_of_processes > MAX_PROCESS_COUNT) {
        printf("Process creation failed. Maximum process count has been reached. You need to wait for other processes to finish executing.\n");
        struct PCB failed_process;
        failed_process.id = -1;
        return failed_process;
    }

    int process_size = generate_random_process_size();

    // Allocate memory for struct PCB
    master_outer_page_table[process_number] = malloc(sizeof(struct PCB));
    if (master_outer_page_table[process_number] == NULL) {
        // Handle memory allocation failure
        fprintf(stderr, "Failed to allocate memory for struct PCB\n");
        exit(EXIT_FAILURE);
    }

    master_outer_page_table[process_number]->id = process_number;
    master_outer_page_table[process_number]->size = process_size;
    master_outer_page_table[process_number]->size_in_memory = 0;

    printf("\nProcess ID: %d\n", master_outer_page_table[process_number]->id);
    printf("Process Size: %d bytes\n", master_outer_page_table[process_number]->size);

    return *master_outer_page_table[process_number];
}


/**
 * @brief Process requests for memory space.
 * 
 * @param process The process requesting for memory space.
 * @return the size of the request granted of type int. If the size is -1, the request wasn't granted.
 */
int memory_request (struct PCB *process) {
    int request = generate_random_request_size(process->size);

    printf("Process %d is requesting for %d bytes of memory...\n", process->id, request);

    if (request < physical_memory_remaining) {
        printf("Request Granted!\n");
        process->size_in_memory = request;
        return process->size_in_memory;
    }
    return -1;
}


/**
 * @brief Find the page number of a process. If a process has multiple pages, this becomes the starting page number (Note that a process with multiple pages is stored in continguous blocks in virtual memory).
 * 
 * @param process The process whose page is to be found
 * @return int The page number of the process. It returns -1 if the process has not been assigned a page.
 */
int find_process_page_number (struct PCB *process) {

    // iterate through virtual memory to find the process' page
    for (int i = 0; i<NO_OF_PAGES; i++) {
        for (int j=0; j<PAGE_SIZE; j++) {
            if (virtual_memory[i][j].process.id == process->id) {
                // find the number of pages the process is occupying
                return virtual_memory[i][j].page_number;
            }
        }
    }

    return -1;
}


/**
 * @brief Find the frame number of a process. If a process has multiple frames, this becomes the starting frame number (Note that a process with multiple frane is stored in continguous blocks in physical memory).
 * 
 * @param process The process whose frame is to be found
 * @return int The frame number of the process. It returns -1 if the process is not in memory.
 */
int find_process_frame_number(struct PCB *process) {
    int page_number = find_process_page_number(process);
    printf("PROCESS %d page number is %d\n", process->id, page_number);

    if (page_number != -1) {
        int inner_page_table_no = page_number / NO_OF_PAGE_TABLE_ENTRIES_IN_PAGE;
        int inner_page_table_offset = page_number % NO_OF_PAGE_TABLE_ENTRIES_IN_PAGE;

        // Check if the page is valid before retrieving the frame number
        if (process->inner_page_tables[inner_page_table_no][inner_page_table_offset].valid) {
            int frame_number = process->inner_page_tables[inner_page_table_no][inner_page_table_offset].frame_number;
            return frame_number;
        }
    }

    return -1;
}


/**
 * @brief Remove a process from physical memory, unassign its pages, and update page tables accordingly.
 * 
 * @param process The process whose memory space is to be deallocated.
 */
void memory_deallocation(struct PCB *process) {

    printf("Process %d has finished executing. Attempting to deallocate memory...\n", process->id);

    int frame_number = find_process_frame_number(process);
    printf("PROCESS %d frame number is %d\n", process->id, frame_number);
    int no_of_frames = process->size / FRAME_SIZE;

    // Free physical memory of the process
    // first check if the process is in physcial memory.
    if (frame_number != -1) {
        for (int i = frame_number; i < frame_number + no_of_frames; i++) {
            for (int j = 0; j < FRAME_SIZE; j++) {
                if (physical_memory[i][j].id != -1) {
                    // Free physical memory by marking the frame as empty
                    physical_memory[i][j].id = -1;
                }
            }
        }
        physical_memory_remaining += process->size;
        printf("Memory has been successfully deallocated! Process %d is no longer in memory. Physical memory remaining is now %d\n", process->id, physical_memory_remaining);

        // Set process's page's number's corresponding frame to -1.
        int page_number = find_process_page_number(process);
        int inner_page_table_no = page_number / NO_OF_PAGE_TABLE_ENTRIES_IN_PAGE;
        int inner_page_table_offset = page_number % NO_OF_PAGE_TABLE_ENTRIES_IN_PAGE;
        process->inner_page_tables[inner_page_table_no][inner_page_table_offset].frame_number = -1;
        process->inner_page_tables[inner_page_table_no][inner_page_table_offset].valid = 0;

        // Free virtual memory of the process
        for (int i = page_number; i < page_number + no_of_frames; i++) {
            for (int j = 0; j < PAGE_SIZE; j++) {
                if (virtual_memory[i][j].process.id != -1) {
                    // Free virtual memory by marking the page as empty
                    virtual_memory[i][j].process.id = -1;
                }
            }
        }
    }

    

}


/**
 * @brief Visualize the inner page tables of a process in a tabular format
 * 
 * @param process The process whose inner page tables are to be visualized
 */
void visualize_inner_page_tables(struct PCB *process) {
    printf("Inner Page Tables Visualization for Process %d:\n", process->id);

    for (int i = 0; i < OUTER_PAGE_TABLE_SIZE; i++) {
        for (int j = 0; j < NO_OF_PAGE_TABLE_ENTRIES_IN_PAGE; j++) {
            printf("(%d,%d) ", process->inner_page_tables[i][j].frame_number, process->inner_page_tables[i][j].valid);
        }
        printf("\n");
    }
    printf("\n");
}
