// #include<stdio.h>
// #include<pthread.h>
// #include<stdlib.h>

// #define THREAD_CNT 3

// // waste some time
// void *count(void *arg) {
//         unsigned long int c = (unsigned long int)arg;
//         int i;
//         for (i = 0; i < c; i++) {
//                 if ((i % 1000000) == 0) {
//                         printf("tid: 0x%x Just counted to %d of %ld\n", \
//                         (unsigned int)pthread_self(), i, c);
//                 }
//         }
//     return arg;
// }

// int main(int argc, char **argv) {
//         pthread_t threads[THREAD_CNT];
//         int i;
//         unsigned long int cnt = 10000000;
//         printf("%u\n",(unsigned int)pthread_self());
//     //create THRAD_CNT threads
//         for(i = 0; i<THREAD_CNT; i++) {
//                 pthread_create(&threads[i], NULL, count, (void *)((i+1)*cnt));
//         }

//     //join all threads ... not important for proj2
//         for(i = 0; i<THREAD_CNT; i++) {
//                 pthread_join(threads[i], NULL);
//         }
//     return 0;
// }

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

pthread_t tid;
pthread_t tid2;
int total = 2;
int destroy = 0;

void * do_something(void *arg)
{
	arg = NULL;
	tls_clone(tid);
	char buffa2[10];
	printf("%u\n",pthread_self());


	int i;
	for(i = 0; i < 10; i++)
	{
		buffa2[i] = i*2;
	}
	tls_write(0, 10, buffa2);
	int destroy = 1;

	char buffer[10];
	tls_read(0, 10, buffer);

	for(i = 0; i < 10; i++)
	{
		printf("buffer2: %d\n",buffer[i]);
	}
	printf("destroy success: %d\n",tls_destroy());

	total--;
	return arg;
}

int main(int argc, const char* argv[] )
{
	tid = pthread_self();
	tls_create(4100);
	char buffer[10];
	int i;
	for(i = 0; i < 10; i++)
	{
		buffer[i] = i;
	}
	tls_write(0, 10, buffer);

	pthread_create(&tid2, NULL, do_something, NULL);

	printf("%d\n",pthread_self());

	while(total > 1)
	{
		
	}
	
	char buffa3[10];

	tls_read(0, 10, buffa3);
	printf("Managed to read... \n");

	for(i = 0; i < 10; i++)
	{
		printf("buffer3 - should be 1-10: %d\n", buffa3[i]);
	}

	printf("return value: %d\n",tls_destroy());
	return 0;
}



// #include <pthread.h>
// #define THREAD_CNT 3


// // waste some time
// void *count1(void *arg) {
//         unsigned long int c = (unsigned long int)arg;
//         int i;
//         for (i = 0; i < c; i++) {
//                 if ((i % 100000) == 0) {
//                         printf("1tid: 0x%x Just counted to %d of %ld\n", 
//                         (unsigned int)pthread_self(), i, c);
//                 }
//         }
//     return arg;
// }

// void *count2(void *arg) {
//         unsigned long int c = (unsigned long int)arg;
//         int i;
//         for (i = 0; i < c; i++) {
//                 if ((i % 100000) == 0) {
//                         printf("3tid: 0x%x Just counted to %d of %ld\n", 
//                         (unsigned int)pthread_self(), i, c);
//                 }
//         }
//     return arg;
// }

// int main(int argc, char **argv) {
//         pthread_t threads[THREAD_CNT];
//         int i;
//         unsigned long int cnt = 20000000;

//     //create THRAD_CNT threads
//         for(i = 0; i<THREAD_CNT; i++) {
//          if(i%2 == 0)
//                 pthread_create(&threads[i], NULL, count1, (void *)((i+1)*cnt));
//             else
//              pthread_create(&threads[i], NULL, count2, (void *)((i+1)*cnt));
//         }

//     //join all threads ... not important for proj2
//         for(i = 0; i<THREAD_CNT; i++) {
//                 pthread_join(threads[i], NULL);
//                 printf("Waiting on thread: %d\n",i+1);
//         }

//     return 0;
// }