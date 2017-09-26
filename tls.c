#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <setjmp.h>
#include <signal.h>
#include <semaphore.h>
#include <sys/mman.h>

//#define NO_PAGES = 256; 

typedef struct thread_local_storage
{	
	pthread_t tid;	
	unsigned int size;				/*	size	in	bytes*/	
	unsigned int page_num;				/*	number	of	pages*/	
	struct page	**pages;				/*	array	of	pointers	to	pages*/	
}TLS;	

struct page
{	
	unsigned int address;			/*	start	address	of	page*/	
	int ref_count;					/*	counter	for	shared	pages*/	
};

typedef struct node
{
	TLS *tls;
	struct node *next; 
}node_t;

int tls_create(unsigned	int	size);
int tls_write(unsigned int offset, unsigned int length, char *buffer);
int tls_read(unsigned int	offset,	unsigned int length, char *buffer);
int tls_destroy();
int tls_clone(pthread_t tid);
void tls_handle_page_fault(int sig, siginfo_t *si,void *context);
void tls_protect(struct	page *p);
void tls_unprotect(struct page *p);


//global variables
int page_size;
node_t * head;
int initialized = 0;

void tls_init()	
{	
		struct sigaction sigact;	
		/*	get	the	size	of	a	page	*/	
		page_size =	getpagesize();	
		/*	install	the	signal	handler	for	page	faults	(SIGSEGV,	SIGBUS)	*/	
		sigemptyset(&sigact.sa_mask);	
		sigact.sa_flags	=	SA_SIGINFO;	/*	use	extended	signal	handling	*/		
		sigact.sa_sigaction	=	tls_handle_page_fault;	
		sigaction(SIGBUS, &sigact, NULL);	
		sigaction(SIGSEGV, &sigact, NULL);	
		initialized	= 1;	

		//
		node_t * first_node = (node_t*)malloc(sizeof(node_t));//malloc a head node
		first_node->next = NULL;
		head = first_node;
}	

void tls_handle_page_fault(int sig, siginfo_t *si,void *context)
{
	unsigned int p_fault;
	p_fault	= ((unsigned int)si->si_addr) & ~(page_size	-1);//assigning the fault

	node_t *current = head;
	while(current->next != NULL)//loop through the linked list
	{
		current = current->next;
		unsigned int current_tls_pagenum = current->tls->page_num;
		int i = 0;
		for(i; i< current_tls_pagenum;i++)
		{
			if (current->tls->pages[i]->address == p_fault)//if its a real segfault
			{	
		 		pthread_exit(NULL);	//exit
		 	}
		 }
	 	//else
	 	signal(SIGSEGV, SIG_DFL);	
		signal(SIGBUS, SIG_DFL);	
		raise(sig);	
	}


}

int tls_create(unsigned	int	size)
{
	if(initialized == 0)//if not initialized 
	{
		tls_init();//call init
	}

	node_t *current = head;
	while(current->next != NULL)//check if the tls exists
	{
		current = current->next;
		if (current->tls->tid == pthread_self())
		{
			return -1;
		}
	}
	if(size < 0)// less than 0
	{
		return -1;
	}

	TLS* new_tls = (TLS*)calloc(1,sizeof(TLS));//calloc a new TLS

	new_tls->tid = pthread_self();//set the id
	new_tls->page_num = ((size-1)/(page_size+1));//find the number of pages
	new_tls->size = size;//set the size;
	new_tls->pages = (struct page**)malloc(sizeof(struct page*) * new_tls->page_num);//create an array for the pages

	//Allocate all pages for this TLS
	int cnt = 0;
	for (cnt = 0; cnt < new_tls->page_num; cnt++) 
	{
		struct page *p;//create a page pointer 
 		p = (struct page *) calloc(1, sizeof(struct page));//allocate a space for that pointer
 		p->address = (unsigned int) mmap(0, page_size, 0, MAP_ANON | MAP_PRIVATE, 0, 0);//creates the pages itself
 		p->ref_count = 1;//set the ref count
 		new_tls->pages[cnt] = p;//put it in the array 
	}

	current = head;//find the lasat place of the 
    while (current->next != NULL) 
    {
        current = current->next;
    }

    //adding the new tls to the linked list
    current->next = malloc(sizeof(node_t));
    current->next->tls = new_tls;
    current->next->next = NULL;

    return 0;
}

int tls_destroy()
{
	node_t *current = head;
	pthread_t thrd_id = pthread_self();
	while(current->next != NULL)//check if the tls exists
	{
		current = current->next;
		if (current->tls->tid == thrd_id)//if the tls exists
		{
			break;
		}
		else if(current->next == NULL && current->tls->tid != thrd_id)//if tls doesnt exust
		{
			return -1;//error
		}
	}

	unsigned int current_tls_pagenum = current->tls->page_num;//get the page numbers
	int i = 0;
	for(i; i< current_tls_pagenum;i++)//loop through the page array
	{
		if(current->tls->pages[i]->ref_count == 1)
		{
		unsigned int d_address = current->tls->pages[i]->address;//get the adress to delete
		munmap((void*)d_address, page_size);//unmap the adress
		}
		else
		{
			current->tls->pages[i]->ref_count--;
		}
	}

	node_t *to_delete = current;//the delete node is the current node
	current = head;//current is the head
	for( ; current->next != to_delete;current = current->next)//loop through the linked list until finding the to delete node
	{
		if(current->next == to_delete)//if its the to delete node
		{
			current->next = to_delete->next;//set the current nodes next to to deletes next
		}
	}
	free(to_delete->tls->pages);//free the array of pages
	free(to_delete->tls);//free the tls
	free(to_delete);//delete the node

	return 0;
}

void tls_protect(struct	page *p)// protecting the page 
{	
	if	(mprotect((void	*)	p->address,	page_size,	0))//form the address to the page size, the memory can not be accesed
	{	
		fprintf(stderr,	"tls_protect: could not	protect	page\n");	
		exit(1);	
	}	
}

void tls_unprotect(struct page *p)	//unprotecting the page
{	
	if (mprotect((void	*)	p->address,	page_size,	PROT_READ	|	PROT_WRITE))//the memory can be read and write
	{	
		fprintf(stderr,	"tls_unprotect:	could	not	unprotect	page\n");	
		exit(1);	
	}	
}	

int tls_read(unsigned int	offset,	unsigned int length, char *buffer)
{
	node_t *current = head;
	pthread_t thrd_id = pthread_self();
	while(current->next != NULL)//check if the tls exists
	{
		current = current->next;
		if (current->tls->tid == thrd_id)//if the tls exists
		{
			break;
		}
		else if(current->next == NULL && current->tls->tid != thrd_id)//if tls doesnt exust
		{
			return -1;//error
		}
	}
	if(offset+length > current->tls->size)//if its greater than the size
	{
		return -1;
	}

	unsigned int current_tls_pagenum = current->tls->page_num;//get the page numbers
	int i = 0;
	for(i; i< current_tls_pagenum;i++)//loop through the page array
	{
		struct page* unp_page = current->tls->pages[i];//get the 
		tls_unprotect(unp_page);//unprotect the adress
	}

	int cnt = 0;
	int idx;
	for	(cnt, idx = offset; idx <(offset+length); ++cnt, ++idx)//read operation
	{	
		struct page	*p;	//create a page pointer
		unsigned int pn, poff;	//create the unsigned integer
		pn = idx / page_size;	//you get the pagenumber
		poff = idx % page_size;	// and the page offset
		p = current->tls->pages[pn]; //access that page
		char* src = ((char *) p->address) + poff; //get the characters
		buffer[cnt]	= *src;	//write them to the buffer
	}

	i = 0;
	for(i; i< current_tls_pagenum;i++)//loop through the page array
	{
		struct page* unp_page = current->tls->pages[i];//get the 
		tls_protect(unp_page);//protect the adress
	}

	return 0;
}

int tls_write(unsigned int offset, unsigned int length, char *buffer)
{
	node_t *current = head;
	pthread_t thrd_id = pthread_self();
	while(current->next != NULL)//check if the tls exists
	{
		current = current->next;
		if (current->tls->tid == thrd_id)//if the tls exists
		{
			break;
		}
		else if(current->next == NULL && current->tls->tid != thrd_id)//if tls doesnt exust
		{
			return -1;//error
		}
	}
	if(offset+length > current->tls->size)//if its greater than the size
	{
		return -1;
	}

	unsigned int current_tls_pagenum = current->tls->page_num;//get the page numbers
	int i = 0;
	for(i; i< current_tls_pagenum;i++)//loop through the page array
	{
		struct page* unp_page = current->tls->pages[i];//get the 
		tls_unprotect(unp_page);//unprotect the adress
	}

	int cnt, idx;
	for (cnt = 0, idx = offset; idx < (offset + length); ++cnt, ++idx)//write operation
	{
		struct page *p, *copy;//create a page pointers
		unsigned int pn, poff;
		pn = idx / page_size;//get the pagenumber
		poff = idx % page_size;//get the page offset
		p = current->tls->pages[pn];//get the page from the array

		if (p->ref_count > 1) //copy on write operation
		{
			copy = (struct page *) calloc(1, sizeof(struct page));//create a memory space for copy
			copy->address = (unsigned int) mmap(0, page_size, PROT_WRITE, MAP_ANON | MAP_PRIVATE, 0, 0);//create the copy page
			printf("[+] allocated %x\n", copy->address);
			copy->ref_count = 1;//set the ref count
			current->tls->pages[pn] = copy;//change the page to the copy page
			 /* update original page */
			
			char *j = (char*) p->address;
			int k = p->address + page_size;
			int l = 0;
			int t = p->address;
			for(t; t < k; j++)//copy the actual contents of the page
			{
				*((char*)copy->address+l) = *j;
				l++;
			} 
			p->ref_count--;//decrease the ref count of the current page
			tls_protect(p);//protect the current page
			p = copy;//change the current page to the copied page
		}

		char* dst = ((char *) p->address) + poff;//get the page
 		*dst = buffer[cnt];//change the page contents witht the buffer
	}

	i = 0;
	for(i; i< current_tls_pagenum;i++)//loop through the page array
	{
		struct page* unp_page = current->tls->pages[i];//get the 
		tls_protect(unp_page);//protect the adress
	}

	return 0;
}

int tls_clone(pthread_t tid)
{
	node_t *current = head;
	pthread_t thrd_id = pthread_self();
	while(current->next != NULL)//check if the tls exists for current thread
	{
		current = current->next;
		if (current->tls->tid == thrd_id)//if the tls exists
		{
			break;
		}
		else if(current->next == NULL && current->tls->tid != thrd_id)//if tls doesnt exust
		{
			return -1;//error
		}
	}

	node_t *target = head;
	while(target->next != NULL)//check if the tls exists for the target thread
	{
		target = target->next;
		if (target->tls->tid == tid)//if the tls exists
		{
			break;
		}
		else if(target->next == NULL && target->tls->tid != tid)//if tls doesnt exist
		{
			return -1;//error
		}
	}

	TLS* new_tls = (TLS*)calloc(1,sizeof(TLS));//calloc a new TLS

	new_tls->tid = pthread_self();//set the id
	new_tls->page_num = target->tls->page_num;//find the number of pages
	new_tls->size = target->tls->size;//set the size;
	new_tls->pages = (struct page**)malloc(sizeof(struct page*) * new_tls->page_num);//create an array for the pages

	int cnt = 0;
	for (cnt = 0; cnt < new_tls->page_num; cnt++)//loop throught the array
	{
		new_tls->pages[cnt] = target->tls->pages[cnt];//point the pointers to the right pages
		new_tls->pages[cnt]->ref_count++;//inreace the ref count
	}

	current = head;//find the lasat place of the 
    while (current->next != NULL) 
    {
        current = current->next;
    }

    //adding the new tls to the linked list
    current->next = malloc(sizeof(node_t));
    current->next->tls = new_tls;
    current->next->next = NULL;

    return 0;
}



