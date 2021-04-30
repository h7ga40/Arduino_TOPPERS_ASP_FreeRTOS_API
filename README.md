# FreeRTOSをTOPPERSで実装する

Arduinoライブラリとして[TOPPERS/ASP](https://www.toppers.jp/asp-kernel.html)を使えるようにした[プロジェクト](https://github.com/exshonda/Arduino_TOPPERS_ASP)があります。

このプロジェクトのターゲットデバイスは[Wio Terminal](https://wiki.seeedstudio.com/jp/Wio-Terminal-Getting-Started/)で、Wi-FiやBLEが使えます。ただ、それを使うためには[FreeRTOS](https://github.com/Seeed-Studio/Seeed_Arduino_FreeRTOS)が必要で、先述のTOPPERS/ASPと競合してしまいます。

このライブラリは、TOPPERS/ASPライブラリを使ったArduino
スケッチでもWiFiやBLEを使えるようにするため、FreeRTOSの一部のAPIをTOPPERSで実装したものです。

Wio TerminalでWi-Fiを使う場合は、[rpcWifi](https://github.com/Seeed-Studio/Seeed_Arduino_rpcWiFi)というライブラリを使い、BLEを使う場合は[rpcBLE](https://github.com/Seeed-Studio/Seeed_Arduino_rpcBLE)というライブラリを使います。
この二つのライブラリは、[rpcUnified](https://github.com/Seeed-Studio/Seeed_Arduino_rpcUnified)というライブラリを使っています。
この３つのライブラリが使っているFreeRTOSのAPIに絞って、TOPPERS/ASPで実装しました。

## 実装手順

今回は、WiFiやBLEを使ったスケッチをFreeRTOSを省いてビルドし、未定義シンボルとしてリンクエラーの出たFreeRTOSのAPIだけを対象に、TOPPERS/ASPで実装することにしました。

|未定義シンボル|FreeRTOS API|実装方法|
|-|-|-|
|`pvPortMalloc`||`malloc`|
|`vPortFree`||`free`|
|`vPortEnterCritical`||`loc_cpu`|
|`vPortExitCritical`||`unl_cpu`|
|`vTaskStartScheduler`||`StartToppersASP`|
|`xTaskCreate`||`cre_tsk`|
|`vTaskDelete`||~~`del_tsk`~~|
|`vTaskSuspend`||`sus_tsk`|
|`vTaskDelay`||`dly_tsk`|
|`xTaskGetCurrentTaskHandle`||`get_tid`|
|`xQueueGenericCreate`|`xSemaphoreCreateBinary`|`cre_sem`|
|`vQueueDelete`|`vSemaphoreDelete`|~~`del_sem`~~|
|`xQueueCreateCountingSemaphore`|`xSemaphoreCreateCounting`|`cre_sem`|
|`xQueueCreateMutex`|`xSemaphoreCreateMutex`|`cre_sem`|
|`xQueueGenericSend`|`xSemaphoreGive`|`sig_sem`|
|`xQueueSemaphoreTake`|`xSemaphoreTake`|`wai_sem`|
|`xQueueGiveMutexRecursive`|`xSemaphoreGiveRecursive`|`sig_sem`|
|`xQueueTakeMutexRecursive`|`xSemaphoreTakeRecursive`|`wai_sem`|
|`xEventGroupCreate`||`cre_flg`|
|`xEventGroupSetBits`||`set_flg`|
|`xEventGroupClearBits`||`clr_flg`|
|`xEventGroupGetBits`||`ref_flg`|
|`xEventGroupWaitBits`||`wai_flg`|

## FreeRTOSのAPIリネーム

FreeRTOSのAPIは、ユーザー向けの関数名と内部で使用している関数名があるようで、下記の表のようにリネームされています。

|マクロ名|関数名|
|-|-|
|`xSemaphoreCreateMutex`|`xQueueGenericCreate`|
|`xSemaphoreCreateCounting`|`xQueueCreateCountingSemaphore`|
|`xSemaphoreCreateBinary`|`xQueueGenericCreate`|
|`vSemaphoreDelete`|`vQueueDelete`|
|`xSemaphoreTakeRecursive`|`xQueueTakeMutexRecursive`|
|`xSemaphoreGiveRecursive`|`xQueueGiveMutexRecursive`|
|`xSemaphoreTake`|`xQueueSemaphoreTake`|
|`xSemaphoreGive`|`xQueueGenericSend`|
|`xSemaphoreGiveFromISR`|`xQueueGiveFromISR`|

このライブラリでは、ユーザー向けの関数名で実装しました。

## タスク関連APIの実装

|FreeRTOS API|実装方法|
|-|-|
|`xTaskCreate`|`cre_tsk`|
|`vTaskDelete`|~~`del_tsk`~~|
|`vTaskSuspend`|`sus_tsk`|
|`vTaskDelay`|`dly_tsk`|
|`xTaskGetCurrentTaskHandle`|`get_tid`|

FreeRTOSのタスクは`TaskHandle_t`で識別され、`FreeRTOS\src\task.h`に以下のように定義されます。

```c
typedef struct tskTaskControlBlock* TaskHandle_t;
```

`struct tskTaskControlBlock`は`FreeRTOS\src\task.c`で定義されるので、APIとしての定義では中身はいりません。
TOPPERSでの実装では`struct tskTaskControlBlock`を、TOPPERSのタスクと対応付けるために使用します。また、FreeRTOSではタスクに名前を付けられるため、名前用の領域も持たせます。

```c
typedef struct tskTaskControlBlock
{
  ID tskid;
  const char *pcName;
} TCB_t;

TCB_t FreeRTOS_TCB[10];
```

ToppersASPライブラリで用意されているタスク数10個分を、上記の構造体の配列として持たせ、FreeRTOSとTOPPERSのタスクの対応付けのテーブル`FreeRTOS_TCB`として領域を確保しておきます。

FreeRTOS APIが呼ばれた場合は、`TaskHandle_t`のメンバ変数`tskid`で、TOPPERSのAPIを呼び出します。

`tskid`から`TaskHandle_t`を特定するには、`FreeRTOS_TCB`の中の`tskid`が等しいものを探して取得します。
ToppersASPライブラリで`tskid`に使用する値は、`TASK1`から`TASK10`までの連続値で割り振ってあるので、`FreeRTOS_TCB[tskid - TASK1]`でも取得できます。

### xTaskCreate

FreeRTOSのAPIは下記の通りです。

```c
BaseType_t xTaskCreate(TaskFunction_t pxTaskCode, const char *const pcName, const configSTACK_DEPTH_TYPE usStackDepth, void *const pvParameters, UBaseType_t uxPriority, TaskHandle_t *const pxCreatedTask) PRIVILEGED_FUNCTION;
```

|引数|概要|
|-|-|
|`pxTaskCode`|タスクの関数|
|`pcName`|タスクの名前|
|`usStackDepth`|スタックサイズ|
|`pvParameters`|タスクの関数に渡す引数|
|`uxPriority`|タスク優先度|
|`pxCreatedTask`|生成したタスクのハンドルを返す|

TOPPERS/ASPでタスクを生成するAPIは下記の通りです。

```c
extern ER cre_tsk(ID tskid, const T_CTSK *pk_ctsk) throw();
```

|引数|概要|
|-|-|
|`tskid`|タスクID|
|`pk_ctsk`|タスク生成情報|

引数のタスクID（`tskid`）は、タスク対応付けのテーブル`FreeRTOS_TCB`で、メンバ変数の`tskid`が`0`のものを検索して、割り当てます。`tskid`の値は割り当てた`FreeRTOS_TCB`のインデックスに`TASK1`を足した値を使用します。

|メンバ変数|概要|設定値|
|-|-|-|
|`tskatr`|タスク属性|`TA_ACT`（タスクを起動された状態で生成）|
|`exinf`|タスクの拡張情報|`pvParameters`|
|`task`|タスクのメインルーチンの先頭番地|`pxTaskCode`|
|`itskpri`|タスクの起動時優先度|`TMAX_TPRI - uxPriority`|
|`stksz`|タスクのスタック領域のサイズ|`usStackDepth * sizeof(StackType_t)`|
|`stk`|タスクのスタック領域の先頭番地|`NULL`（カーネルで割り当て）|

引数のスタックサイズは、Byteサイズを指定するのではなく、`StackType_t`分の長さの個数になっているので、乗算してByteサイズにしたものを設定します。

FreeRTOSのタスク優先度は２種類あり、「Generic Method
」と「Architecture Optimized Method」があります。
`FreeRTOS\src\boards\SAMD51\portmacro.h`で「Architecture Optimized Method」が選択されています。
この場合、優先度は0～31の値が取れ、値が大きい方が優先度も高いということになります。

```c
/* Architecture specific optimisations. */
#ifndef configUSE_PORT_OPTIMISED_TASK_SELECTION
  #define configUSE_PORT_OPTIMISED_TASK_SELECTION 1
#endif
```

一方TOPPERSでは、値が小さい方が優先度が高いので、TOPPERSの最低優先度の値`TMAX_TPRI`から、引数で与えられた`uxPriority`の差としています。
また、`TMIN_TPRI`より小さい値は取れないので、計算の結果がこの値より小さいときは、`TMIN_TPRI`とします。

### vTaskDelete

FreeRTOSのAPIは下記の通りです。

```c
void vTaskDelete(TaskHandle_t xTaskToDelete) PRIVILEGED_FUNCTION;
```

TOPPERS/ASPでのタスクを削除するAPIは下記の通りです。

```c
ER del_tsk(ID tskid) throw();
```

これはToppersASPライブラリでは提供されていないので、実装していません。

### vTaskSuspend

FreeRTOSのAPIは下記の通りです。

```c
void vTaskSuspend(TaskHandle_t xTaskToSuspend) PRIVILEGED_FUNCTION;
```

TOPPERS/ASPのタスクをサスペンドするAPIは下記の通りです。

```c
ER sus_tsk(ID tskid) throw();
```

引数の`xTaskToSuspend`を前述の方法で`tskid`に変換し、TOPPERS側APIを呼び出します。

### vTaskDelay

FreeRTOSのAPIは下記の通りです。

```c
typedef uint32_t TickType_t;

void vTaskDelay(const TickType_t xTicksToDelay) PRIVILEGED_FUNCTION;
```

TOPPERS/ASPのタスク遅延のAPIは下記の通りです。

```c
typedef uint_t RELTIM;

ER dly_tsk(RELTIM dlytim) throw();
```

`TickType_t`は、`FreeRTOS\src\boards\SAMD51\FreeRTOSConfig.h`で`configUSE_16_BIT_TICKS`が`0`に設定されているので、`uint32_t`が使われます。
両方の取り得る値域は同じで、永遠待ちを示す値も両方とも`0xFFFFFFFF`で同じなので、変換していません。

### xTaskGetCurrentTaskHandle

FreeRTOSのAPIは下記の通りです。

```c
TaskHandle_t xTaskGetCurrentTaskHandle(void) PRIVILEGED_FUNCTION;
```

TOPPERS/ASPの現在のタスクIDを返すAPIは下記の通りです。

```c
ER get_tid(ID *p_tskid) throw();
```

`get_tid`で取得したタスクIDを、前述の方法で`FreeRTOS_TCB`から取得し、戻り値として返します。

## セマフォ関連APIの実装

|FreeRTOS API|実装方法|
|-|-|
|`xSemaphoreCreateBinary`|`cre_sem`|
|`xSemaphoreCreateCounting`|`cre_sem`|
|`xSemaphoreCreateMutex`|`cre_sem`|
|`vSemaphoreDelete`|~~`del_sem`~~|
|`xSemaphoreTake`|`wai_sem`|
|`xSemaphoreGive`|`sig_sem`|
|`xSemaphoreTakeRecursive`|`wai_sem`|
|`xSemaphoreGiveRecursive`|`sig_sem`|

FreeRTOSのセマフォは幾つか種類があり、ミーテックス、バイナリセマフォ、カウンティングセマフォが使われています。

ミーテックスは優先度継承メカニズムを含んでいますが、バイナリセマフォは優先度継承メカニズムは含んでいません。

FreeRTOSのセマフォのAPIは、内部では待ち行列処理の関数にマクロでリネームされていますが、TOPPERSでの実装ではAPIのまま使っています。FreeRTOSのヘッダーファイルでビルドしたものは、待ち行列処理の関数を必要とするので、ライブラリのみ変更した場合リンクエラーになります。

FreeRTOSのセマフォは`SemaphoreHandle_t`で識別され、`FreeRTOS\src\semphr.h`に以下のように定義されています。

```c
typedef QueueHandle_t SemaphoreHandle_t;
```

また、`QueueHandle_t`は`FreeRTOS\src\queue.h`に以下のように定義されています。

```c
typedef struct QueueDefinition* QueueHandle_t;
```

`struct QueueDefinition`は`FreeRTOS\src\queue.c`で定義されるので、APIとしての定義では中身はいりません。
TOPPERSでの実装では`struct QueueDefinition`を、TOPPERSのセマフォと対応付けるために使用します。

```c
typedef struct QueueDefinition
{
  ID semid;
  uint8_t ucQueueType;
  ID xMutexHolder;
  UBaseType_t uxRecursiveCallCount;
} Queue_t;

Queue_t FreeRTOS_SEM[10];
```

ToppersASPライブラリで用意されているセマフォ数10個分を、上記の構造体の配列として持たせ、FreeRTOSとTOPPERSのセマフォの対応付けのテーブル`FreeRTOS_SEM`として領域を確保しておきます。

FreeRTOS APIが呼ばれた場合は、`SemaphoreHandle_t`のメンバ変数`semid`で、TOPPERSのAPIを呼び出します。

### xSemaphoreCreateBinary

FreeRTOSのAPIは下記の通りです。

```c
#define semSEMAPHORE_QUEUE_ITEM_LENGTH ((uint8_t)0U)

#define xSemaphoreCreateBinary() xQueueGenericCreate((UBaseType_t)1, semSEMAPHORE_QUEUE_ITEM_LENGTH, queueQUEUE_TYPE_BINARY_SEMAPHORE)

QueueHandle_t xQueueGenericCreate(const UBaseType_t uxQueueLength, const UBaseType_t uxItemSize, const uint8_t ucQueueType) PRIVILEGED_FUNCTION;
```

本実装でのFreeRTOS APIは下記のようにしました。

```c
SemaphoreHandle_t xSemaphoreCreateBinary(void) PRIVILEGED_FUNCTION;
```

`xQueueGenericCreate`では、待ち行列を生成するための引数が用意されていますが、`cre_sem`を呼び出すには引数名が分かりにくいため、`xSemaphoreCreateBinary`のAPIで実装しました。
また、`xSemaphoreCreateCounting`と`xSemaphoreCreateMutex`でも共通に使用できるよう、内部関数として`xSemaphoreGenericCreate`を定義しました。

`xSemaphoreGenericCreate`の定義は下記の通りです。

```c
SemaphoreHandle_t xSemaphoreGenericCreate(const UBaseType_t uxMaxCount, const UBaseType_t uxInitialCount)
```

|引数|概要|
|-|-|
|`uxMaxCount`|セマフォの最大資源数|
|`uxInitialCount`|セマフォの初期資源数|

TOPPERS/ASPのセマフォを生成するAPIは下記の通りです。

```c
ER cre_sem(ID semid, const T_CSEM *pk_csem) throw();
```

|引数|概要|
|-|-|
|`semid`|セマフォID|
|`pk_csem`|セマフォ生成情報|

引数のセマフォID（`semid`）は、セマフォ対応付けのテーブル`FreeRTOS_SEM`で、メンバ変数の`semid`が`0`のものを検索して、割り当てます。`semid`の値は割り当てた`FreeRTOS_SEM`のインデックスに`SEM1`を足した値を使用します。

|メンバ変数|概要|設定値|
|-|-|-|
|`sematr`|セマフォ属性|`TA_TPRI`（タスクの待ち行列を優先度順に）|
|`isemcnt`|セマフォの初期資源数|`uxInitialCount`（バイナリセマフォの場合は、`0`）|
|`maxsem`|セマフォの最大資源数|`uxMaxCount`（バイナリセマフォの場合は、`1`）|

### xSemaphoreCreateCounting

FreeRTOSのAPIは下記の通りです。

```c
#define xSemaphoreCreateCounting(uxMaxCount, uxInitialCount) xQueueCreateCountingSemaphore((uxMaxCount), (uxInitialCount))

QueueHandle_t xQueueCreateCountingSemaphore(const UBaseType_t uxMaxCount, const UBaseType_t uxInitialCount) PRIVILEGED_FUNCTION;
```

本実装でのFreeRTOS APIは下記のようにしました。

```c
SemaphoreHandle_t xSemaphoreCreateCounting(const UBaseType_t uxMaxCount, const UBaseType_t uxInitialCount) PRIVILEGED_FUNCTION;
```

`xSemaphoreCreateBinary`と同様に、`xSemaphoreGenericCreate`を呼び出しセマフォを生成します。

|引数|概要|設定値|
|-|-|-|
|`uxMaxCount`|セマフォの最大資源数|受け取った`uxMaxCount`|
|`uxInitialCount`|セマフォの初期資源数|受け取った`uxInitialCount`|

### xSemaphoreCreateMutex

FreeRTOSのAPIは下記の通りです。

```c
#define xSemaphoreCreateMutex() xQueueCreateMutex(queueQUEUE_TYPE_MUTEX)

QueueHandle_t xQueueCreateMutex(const uint8_t ucQueueType) PRIVILEGED_FUNCTION;
```

本実装でのFreeRTOS APIは下記のようにしました。

```c
SemaphoreHandle_t xSemaphoreCreateMutex(void) PRIVILEGED_FUNCTION;
```

`xSemaphoreCreateBinary`と同様に、`xSemaphoreGenericCreate`を呼び出しセマフォを生成します。

|引数|概要|設定値|
|-|-|-|
|`uxMaxCount`|セマフォの最大資源数|`1`|
|`uxInitialCount`|セマフォの初期資源数|`1`|

### vSemaphoreDelete

FreeRTOSのAPIは下記の通りです。

```c
#define vSemaphoreDelete(xSemaphore) vQueueDelete((QueueHandle_t)(xSemaphore))
void vSemaphoreDelete(QueueHandle_t xQueue) PRIVILEGED_FUNCTION;
```

本実装でのFreeRTOS APIは下記のようにしました。

```c
void vSemaphoreDelete(SemaphoreHandle_t xSemaphore) PRIVILEGED_FUNCTION;
```

TOPPERS/ASPのセマフォを削除するAPIは下記の通りです。

```c
ER del_sem(ID semid) throw();
```

これはToppersASPライブラリでは提供されていないので、実装していません。

### xSemaphoreTake

FreeRTOSのAPIは下記の通りです。

```c
#define xSemaphoreTake(xSemaphore, xBlockTime) xQueueSemaphoreTake((xSemaphore), (xBlockTime))

BaseType_t xQueueSemaphoreTake(QueueHandle_t xQueue, TickType_t xTicksToWait) PRIVILEGED_FUNCTION;
```

本実装でのFreeRTOS APIは下記のようにしました。

```c
BaseType_t xSemaphoreTake(SemaphoreHandle_t xSemaphore, TickType_t xTicksToWait) PRIVILEGED_FUNCTION;
```

TOPPERS/ASPのセマフォを取得するAPIは下記の通りです。

```c
ER twai_sem(ID semid, TMO tmout) throw();
```

|引数|概要|設定値|
|-|-|-|
|`semid`|セマフォID|`xSemaphore->semid`|
|`tmout`|タイムアウト値|`xTicksToWait`|

種類がミューテックスでセマフォが獲得できた場合、再入を可能とするため、タスクIDを`xSemaphore`に記録しておきます。

### xSemaphoreGive

FreeRTOSのAPIは下記の通りです。

```c
#define semGIVE_BLOCK_TIME ((TickType_t)0U)

#define xSemaphoreGive(xSemaphore) xQueueGenericSend((QueueHandle_t)(xSemaphore), NULL, semGIVE_BLOCK_TIME, queueSEND_TO_BACK)

BaseType_t xQueueGenericSend(QueueHandle_t xQueue, const void* const pvItemToQueue, TickType_t xTicksToWait, const BaseType_t xCopyPosition) PRIVILEGED_FUNCTION;
```

本実装でのFreeRTOS APIは下記のようにしました。

```c
BaseType_t xSemaphoreGive(SemaphoreHandle_t xSemaphore) PRIVILEGED_FUNCTION;
```

TOPPERS/ASPのセマフォに返却するAPIは下記の通りです。

```c
ER sig_sem(ID semid) throw();
```

|引数|概要|設定値|
|-|-|-|
|`semid`|セマフォID|`xSemaphore->semid`|

FreeRTOSの実装では待ち行列用の`xQueueGenericSend`が使われていますが、マクロによって引数の一部が固定値で呼ばれているため、TOPPERS実装ではセマフォの返却として簡易的に実装しました。

種類がミューテックスの場合、再入の最後に呼ばれるため、`xSemaphore`のタスクIDを消しておきます。

### xSemaphoreTakeRecursive

FreeRTOSのAPIは下記の通りです。

```c
#define xSemaphoreTakeRecursive(xMutex, xBlockTime) xQueueTakeMutexRecursive((xMutex), (xBlockTime))

BaseType_t xQueueTakeMutexRecursive(QueueHandle_t xMutex, TickType_t xTicksToWait) PRIVILEGED_FUNCTION;
```

本実装でのFreeRTOS APIは下記のようにしました。

```c
BaseType_t xSemaphoreTakeRecursive(SemaphoreHandle_t xMutex, TickType_t xTicksToWait) PRIVILEGED_FUNCTION;
```

ミューテックス用の再入可能な呼び出しのための処理を行います。
最初の呼び出しの場合`xSemaphoreTake`を呼び出します。
再入であれば、`xMutex`のカウンターを加算します。

**優先度継承については考慮していません。**

### xSemaphoreGiveRecursive

FreeRTOSのAPIは下記の通りです。

```c
#define xSemaphoreGiveRecursive(xMutex) xQueueGiveMutexRecursive((xMutex))

BaseType_t xQueueGiveMutexRecursive(QueueHandle_t xMutex) PRIVILEGED_FUNCTION;
```

本実装でのFreeRTOS APIは下記のようにしました。

```c
BaseType_t xSemaphoreGiveRecursive(SemaphoreHandle_t xMutex) PRIVILEGED_FUNCTION;
```

ミューテックス用の再入可能な呼び出しのための処理を行います。
`xMutex`のカウンターを減算し、再入の最後の呼び出しの場合は`xSemaphoreGive`を呼び出します。

**優先度継承については考慮していません。**

## イベントフラグ関連APIの実装

|FreeRTOS API|実装方法|
|-|-|
|`xEventGroupCreate`|`cre_flg`|
|`xEventGroupSetBits`|`set_flg`|
|`xEventGroupClearBits`|`clr_flg`|
|`xEventGroupGetBits`|`ref_flg`|
|`xEventGroupWaitBits`|`wai_flg`|

FreeRTOSのイベントグループは`EventGroupHandle_t`で識別され、`FreeRTOS\src\event_groups.h`に以下のように定義されます。

```c
typedef struct EventGroupDef_t* EventGroupDef_t;
```

`struct EventGroupDef_t`は`FreeRTOS\src\event_groups.c`で定義されるので、APIとしての定義では中身はいりません。
TOPPERSでの実装では`struct EventGroupDef_t`を、TOPPERSのイベントグループと対応付けるために使用します。

```c
typedef struct EventGroupDef_t
{
  ID flgid;
} EventGroup_t;

EventGroup_t FreeRTOS_FLG[10];
```

ToppersASPライブラリで用意されているイベントグループ数10個分を、上記の構造体の配列として持たせ、FreeRTOSとTOPPERSのイベントグループの対応付けのテーブル`FreeRTOS_FLG`として領域を確保しておきます。

FreeRTOS APIが呼ばれた場合は、`EventGroupHandle_t`のメンバ変数`flgid`で、TOPPERSのAPIを呼び出します。

### xEventGroupCreate

FreeRTOSのAPIは下記の通りです。

```c
EventGroupHandle_t xEventGroupCreate(void) PRIVILEGED_FUNCTION;
```

TOPPERSでイベントグループに相当します。イベントフラグを生成するAPIは下記の通りです。

```c
extern ER cre_flg(ID flgid, const T_CFLG *pk_cflg) throw();
```

|引数|概要|
|-|-|
|`flgid`|イベントグループID|
|`pk_cflg`|イベントグループ生成情報|

引数のイベントグループID（`flgid`）は、イベントグループ対応付けのテーブル`FreeRTOS_FLG`で、メンバ変数の`flgid`が`0`のものを検索して、割り当てます。`flgid`の値は割り当てた`FreeRTOS_FLG`のインデックスに`TASK1`を足した値を使用します。

|メンバ変数|概要|設定値|
|-|-|-|
|`flgatr`|イベントグループ属性|`TA_TPRI`（タスクの待ち行列を優先度順に）|
|`iflgptn`|イベントフラグの初期ビットパターン|`0`|

### xEventGroupSetBits

FreeRTOSのAPIは下記の通りです。

```c
EventBits_t xEventGroupSetBits(EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToSet) PRIVILEGED_FUNCTION;
```

TOPPERS/ASPのビットパターンをセットするAPIは下記の通りです。

```c
ER set_flg(ID flgid, FLGPTN setptn) throw();
```

|引数|概要|設定値|
|-|-|-|
|`flgid`|イベントフラグID|`xEventGroup->flgid`|
|`setptn`|ビットパターン|`uxBitsToSet`|

`xEventGroupSetBits`の戻り値は`uxBitsToSet`を適用した後の値なので、`ref_flg`を呼び出し`set_flg`後の値を取得して戻り値の値としています。

`set_flg`と`ref_flg`は`cpu_loc`した状態では呼び出せないため、両者の実行の間に割り込まれ値が変わっている場合、FreeRTOSと同様の結果を得られない可能性があります。

### xEventGroupClearBits

FreeRTOSのAPIは下記の通りです。

```c
EventBits_t xEventGroupClearBits(EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToClear) PRIVILEGED_FUNCTION;
```

TOPPERS/ASPのビットパターンをクリアするAPIは下記の通りです。

```c
ER clr_flg(ID flgid, FLGPTN clrptn) throw();
```

|引数|概要|設定値|
|-|-|-|
|`flgid`|イベントフラグID|`xEventGroup->flgid`|
|`clrptn`|ビットパターン|`~uxBitsToClear`|

`clrptn`はクリアするためのビットマスクとなっているので、反転しています。

`xEventGroupClearBits`の戻り値は`uxBitsToClear`を適用する前の値なので、`ref_flg`を呼び出し`clr_flg`前の値を取得して戻り値の値としています。

`ref_flg`と`clr_flg`は`cpu_loc`した状態では呼び出せないため、両者の実行の間に割り込まれ値が変わっている場合、FreeRTOSと同様の結果を得られない可能性があります。

### xEventGroupGetBits

FreeRTOSのAPIは下記の通りです。

```c
#define xEventGroupGetBits(xEventGroup) xEventGroupClearBits(xEventGroup, 0)
```

本実装でのFreeRTOS APIは下記のようにしました。

```c
EventBits_t xEventGroupGetBits( EventGroupHandle_t xEventGroup );
```

TOPPERS/ASPのビットパターンを取得するAPIは下記の通りです。

```c
ER ref_flg(ID flgid, T_RFLG *pk_rflg) throw();
```

|引数|概要|設定値|
|-|-|-|
|`flgid`|イベントフラグID|`xEventGroup->flgid`|
|`pk_rflg`|イベントフラグ参照データ||

FreeRTOSの実装では、`xEventGroupClearBits`をクリアビットなしで呼び、その戻り値を利用する実装だが、TOPPERSの実装では`clr_flg`は呼び出さず、`ref_flg`だけで実装しました。

### xEventGroupWaitBits

FreeRTOSのAPIは下記の通りです。

```c
EventBits_t xEventGroupWaitBits(EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToWaitFor, const BaseType_t xClearOnExit, const BaseType_t xWaitForAllBits, TickType_t xTicksToWait) PRIVILEGED_FUNCTION;
```

TOPPERS/ASPのビットパターンを待つAPIは下記の通りです。

```c
ER twai_flg(ID flgid, FLGPTN waiptn, MODE wfmode, FLGPTN *p_flgptn, TMO tmout) throw();
```

|引数|概要|設定値|
|-|-|-|
|`flgid`|イベントフラグID|`xEventGroup->flgid`|
|`waiptn`|イベントフラグ参照データ|`uxBitsToWaitFor`|
|`wfmode`|待ちモード|`xWaitForAllBits`を元に`TWF_ANDW`か`TWF_ORW`を設定|
|`p_flgptn`|待ち解除時のビットパターン||
|`tmout`|タイムアウト|`xTicksToWait`|

`xClearOnExit`に対するTOPPERSの定義は`TA_CLR`があるが、イベントフラグの属性として生成時に指定するものとなっているため、APIの呼び出しごとに変更できません。そのため、`xClearOnExit`をが指定された場合は、`clr_flg`を`~uxBitsToWaitFor`で呼び出しています。

## そのほかのAPIの実装

|FreeRTOS API|実装方法|
|-|-|
|`pvPortMalloc`|`malloc`|
|`vPortFree`|`free`|
|`vPortEnterCritical`|`loc_cpu`|
|`vPortExitCritical`|`unl_cpu`|
|`vTaskStartScheduler`|`StartToppersASP`|

### pvPortMalloc

FreeRTOSのAPIは下記の通りです。

```c
void *pvPortMalloc( size_t xSize ) PRIVILEGED_FUNCTION;
```

これは標準ライブラリの`malloc`を利用します。

### vPortFree

FreeRTOSのAPIは下記の通りです。

```c
void vPortFree( void *pv ) PRIVILEGED_FUNCTION;
```

これは標準ライブラリの`free`を利用します。

### vPortEnterCritical

FreeRTOSのAPIは下記の通りです。

```c
void vPortEnterCritical(void);
```

TOPPERS/ASPの割り込み禁止にするAPIは下記の通りです。

```c
ER loc_cpu(void) throw();
```

`vPortEnterCritical`は再入可能となっているので、グローバル変数で再入カウント`lock_count`を用意し、最初に呼ばれたときに`loc_cpu`を呼び出します。

### vPortExitCritical

FreeRTOSのAPIは下記の通りです。

```c
void vPortExitCritical(void);
```

TOPPERS/ASPの割り込み許可にするAPIは下記の通りです。

```c
ER unl_cpu(void) throw();
```

`vPortExitCritical`は再入可能となっているので、グローバル変数で再入カウント`lock_count`を用意し、最後に呼ばれたときに`unl_cpu`を呼び出します。

### vTaskStartScheduler

FreeRTOSのAPIは下記の通りです。

```c
void vTaskStartScheduler(void) PRIVILEGED_FUNCTION;
```

ToppersASPライブラリのカーネルを起動するAPIは下記の通りです。

```c
void StartToppersASP(void);
```

## 参考

[FreeRTOS の概要（自動翻訳 無保証）](http://www.azusa-st.com/kjm/FreeRtos/FreeRTOS.html)

[Mastering the FreeRTOS Real Time Kernel-A Hands-On Tutorial Guildの日本語訳](https://qiita.com/azuki_bar/items/91e12949737f460dbc11)
