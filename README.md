# pintos

## MLFQ

### 规则

- 新建线程首先进入最高优先级的就绪队列
- 在同一就绪队列中，按照优先级高低调度，相同优先级则FCFS
- 为每个就绪队列分配一个在该就绪队列中总共能使用的时间片
- 若线程在其所在的就绪队列中剩余时间片为0则降级，否则其就绪队列序号不变
- 线程调度时优先选择最高优先级的就绪队列，在其为空时才会选择第二高优先级的就绪队列，依次类推



### 细节

- 目前使用的MLFQ是非抢占式的
- 线程用完在某一就绪队列中的时间片就会降级，在调度中其就绪队列不变并不会重新为其分配时间片，防止某些线程在不断在时间片用完前IO申请来欺骗调度器以维持在高优先级
- 在一定时间后将所有就绪线程重新载入最高优先级就绪队列中，防止饥饿



### 修改文件及内容

#### thread.h

- 定义就绪队列数`MLFQ_SIZE`
- 修改thread结构体，新加field
  - int mlfq[MLFQ_SIZE+1]
  - struct list_elem ready_elem

#### thread.c

- 120行之前加入mlfq信息以及相关操作函数声明
- mlfq相关操作函数定义
- 修改函数：
  - thread_tick()
  - thread_unblock()
  - thread_yield()
  - next_thread_to_run()
  - init_thread()



#### interrupt.h

- 新建函数声明`intr_emerge_on_return`



#### interrupt.c

- 新建变量`bool emerge_on_return`
- 新建函数定义`intr_emerge_on_return`
- 修改函数`intr_handler`结尾部分

