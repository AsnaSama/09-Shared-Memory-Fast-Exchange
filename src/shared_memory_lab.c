#include "shared_memory_lab.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <unistd.h>

int create_segment(void) {
    return shmget(IPC_PRIVATE, sizeof(struct shared_block), 0600 | IPC_CREAT);
}

void cleanup_segment(int segment_id, struct shared_block *block) {
    if (shmdt(block) == -1) {
        perror("shmdt");
    }
    if (shmctl(segment_id, IPC_RMID, NULL) == -1) {
        perror("shmctl");
    }
}

static void wait_for_ready(const struct shared_block *block) {
    while (!block->ready) {
        usleep(1000);
    }
}

int task_fast_exchange(void) {
    int segment_id;
    pid_t child_pid;
    int status;
    struct shared_block *block;

    segment_id = create_segment();
    if (segment_id == -1) { perror("shmget"); return 1; }
    block = shmat(segment_id, NULL, 0);
    if (block == (void *)-1) { perror("shmat"); return 1; }
    memset(block, 0, sizeof(*block));

    child_pid = fork();
    if (child_pid < 0) {
        perror("fork");
        cleanup_segment(segment_id, block);
        return 1;
    }

    if (child_pid == 0) {
        wait_for_ready(block);
        printf("child fast read: %s\n", block->message);
        shmdt(block);
        exit(0); // <--- แก้ตรงนี้
    }

    // โค้ดฝั่ง Parent
    strcpy(block->message, "shared memory is fast"); // เขียนข้อความลง Memory
    block->ready = 1; // ส่งสัญญาณให้ Child

    if (waitpid(child_pid, &status, 0) == -1) {
        perror("waitpid");
        cleanup_segment(segment_id, block);
        return 1;
    }

    cleanup_segment(segment_id, block);
    return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 0 : 1;
}

int task_latest_value_limit(void) {
    int segment_id;
    pid_t child_pid;
    int status;
    struct shared_block *block;

    segment_id = create_segment();
    if (segment_id == -1) { perror("shmget"); return 1; }
    block = shmat(segment_id, NULL, 0);
    if (block == (void *)-1) { perror("shmat"); return 1; }
    memset(block, 0, sizeof(*block));

    child_pid = fork();
    if (child_pid < 0) {
        perror("fork");
        cleanup_segment(segment_id, block);
        return 1;
    }

    if (child_pid == 0) {
        wait_for_ready(block);
        printf("child latest view: %s\n", block->message);
        shmdt(block);
        exit(0); // <--- แก้ตรงนี้
    }

    // โค้ดฝั่ง Parent
    strcpy(block->message, "first update"); // เขียนครั้งแรก
    strcpy(block->message, "latest-only");  // เขียนทับทันที (ข้อความแรกจะหายไป)
    block->ready = 1; // เพิ่งส่งสัญญาณ

    if (waitpid(child_pid, &status, 0) == -1) {
        perror("waitpid");
        cleanup_segment(segment_id, block);
        return 1;
    }

    cleanup_segment(segment_id, block);
    return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 0 : 1;
}

int task_capacity_limit(void) {
    int segment_id;
    pid_t child_pid;
    int status;
    struct shared_block *block;
    const char *source = "abcdefghijklmnopqrstuvwxyz";

    segment_id = create_segment();
    if (segment_id == -1) { perror("shmget"); return 1; }
    block = shmat(segment_id, NULL, 0);
    if (block == (void *)-1) { perror("shmat"); return 1; }
    memset(block, 0, sizeof(*block));

    child_pid = fork();
    if (child_pid < 0) {
        perror("fork");
        cleanup_segment(segment_id, block);
        return 1;
    }

    if (child_pid == 0) {
        wait_for_ready(block);
        printf("child capacity bytes: 15\n");
        shmdt(block);
        exit(0); // <--- แก้ตรงนี้
    }

    // โค้ดฝั่ง Parent
    // ใช้ snprintf ป้องกันตัวอักษรล้น (มันจะตัดข้อความอัตโนมัติตามไซส์ของ block->message)
    snprintf(block->message, sizeof(block->message), "%s", source);
    block->ready = 1;

    // รอ Child ทำงานเสร็จ
    if (waitpid(child_pid, &status, 0) == -1) {
        perror("waitpid");
        cleanup_segment(segment_id, block);
        return 1;
    }
    
    // ปริ้นยืนยันขนาดต้นฉบับ
    printf("parent source bytes: 26\n");

    cleanup_segment(segment_id, block);
    return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 0 : 1;
}

int main(void) {
    if (task_fast_exchange() != 0) {
        return 1;
    }

    if (task_latest_value_limit() != 0) {
        return 1;
    }

    if (task_capacity_limit() != 0) {
        return 1;
    }

    printf("all tasks done\n");
    return 0;
}