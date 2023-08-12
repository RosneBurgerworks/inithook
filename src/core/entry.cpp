/*
 * entry.cpp
 *
 *  Created on: Oct 3, 2016
 *      Author: nullifiedcat
 */

#include "common.hpp"
#include <pthread.h>
#include <semaphore.h>

#include "hack.hpp"

static bool sem_main_inited;
static sem_t sem_main;
static pthread_t thread_main;

void *MainThread(void *arg)
{
    hack::Initialize();
    logging::Info("Init done...");
    sem_wait(&sem_main);
    logging::Info("Shutting down...");
    hack::Shutdown();
    logging::Shutdown();
    return nullptr;
}

void __attribute__((constructor)) attach()
{
    // Prevent double init
    if (sem_main_inited)
        return;

    sem_init(&sem_main, 0, 0);
    sem_main_inited = true;
    pthread_create(&thread_main, 0, MainThread, 0);
}

void detach()
{
    logging::Info("Detaching");
    sem_post(&sem_main);
    pthread_join(thread_main, 0);
}

void __attribute__((destructor)) deconstruct()
{
    detach();
}

CatCommand cat_detach("detach", "Detach cathook from TF2", []() {
    hack::game_shutdown = false;
    detach();
});
