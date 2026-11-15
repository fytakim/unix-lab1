#include <iostream>
#include <pthread.h>
#include <unistd.h>

bool hasMessage = false;

class Event {
public:
    int number;
};

Event *data = NULL;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void *provider(void *arg)
{
    int i = 1;
    while (i <= 6) {
        sleep(1);
        pthread_mutex_lock(&lock);
        if (!hasMessage) {
            Event *msg = new Event;
            msg->number = i;
            data = msg;
            hasMessage = true;
            std::cout << "Отправитель: событие " << i << " отправлено" << std::endl;
            i++;
            pthread_cond_signal(&cond);
        }
        pthread_mutex_unlock(&lock);
    }
    return NULL;
}

void *consumer(void *arg)
{
    int i = 0;
    while (i < 6) {
        pthread_mutex_lock(&lock);
        while (!hasMessage)
            pthread_cond_wait(&cond, &lock);
        Event *msg = data;
        hasMessage = false;
        std::cout << "Получатель: событие " << msg->number << " обработано" << std::endl;
        delete msg;
        pthread_mutex_unlock(&lock);
        i++;
    }
    return NULL;
}

int main()
{
    pthread_t receiveThread;
    pthread_t sendThread;
    pthread_create(&sendThread, NULL, provider, NULL);
    pthread_create(&receiveThread, NULL, consumer, NULL);
    pthread_join(receiveThread, NULL);
    pthread_join(sendThread, NULL);
    pthread_mutex_destroy(&lock);
    pthread_cond_destroy(&cond);
    return 0;
}
