

/**
 * Cyclone 68000 configuration file
**/


/*
 * If this option is enabled, Microsoft ARMASM compatible output is generated.
 * Otherwise GNU as syntax is used.
 */
#define USE_MS_SYNTAX             0

/*
 * Enable this option if you are going to use Cyclone to emulate Genesis /
 * Mega Drive system. As VDP chip in these systems had control of the bus,
 * several instructions were acting differently, for example TAS did'n have
 * the write-back phase. That will be emulated, if this option is enabled.
 * This option also alters timing slightly.
 */
#define CYCLONE_FOR_GENESIS       2

/*
 * This option compresses Cyclone's jumptable. Because of this the executable
 * will be smaller and load slightly faster and less relocations will be needed.
 * This also fixes the crash problem with 0xfffe and 0xffff opcodes.
 * Warning: if you enable this, you MUST call CycloneInit() before calling
 * CycloneRun(), or else it will crash.
 */
#define COMPRESS_JUMPTABLE        1

/*
 * Address mask for memory hadlers. The bits set will be masked out of address
 * parameter, which is passed to r/w memory handlers.
 * Using 0xff000000 means that only 24 least significant bits should be used.
 * Set to 0 if you want to mask unused address bits in the memory handlers yourself.
 */
#define MEMHANDLERS_ADDR_MASK     0

/*
 * Cyclone keeps the 4 least significant bits of SR, PC+membase and it's cycle
 * count in ARM registers instead of the context for performance reasons. If you for
 * any reason need to access them in your memory handlers, enable the options below,
 * otherwise disable them to improve performance.
 * MEMHANDLERS_NEED_PC updates .pc context field with PC value effective at the time
 * when memhandler was called (opcode address + unknown amount).
 * MEMHANDLERS_NEED_PREV_PC updates .prev_pc context field to currently executed
 * opcode address.
 * Note that .pc and .prev_pc values are always real pointers to memory, so you must
 * subtract .membase to get M68k PC value.
 * Warning: updating PC in memhandlers is dangerous, as Cyclone may internally
 * increment the PC before fetching the next instruction and continue executing
 * at wrong location.
 */
#define MEMHANDLERS_NEED_PC       0
#define MEMHANDLERS_NEED_PREV_PC  0
#define MEMHANDLERS_NEED_FLAGS    0
#define MEMHANDLERS_NEED_CYCLES   1
#define MEMHANDLERS_CHANGE_PC     0
#define MEMHANDLERS_CHANGE_FLAGS  0
#define MEMHANDLERS_CHANGE_CYCLES 0

/*
 * If enabled, Cyclone will call IrqCallback routine from it's context whenever it
 * acknowledges an IRQ. IRQ level is not cleared automatically, do this in your
 * hadler if needed. PC, flags and cycles are valid in the context and can be read.
 * If disabled, it simply clears the IRQ level and continues execution.
 */
#define USE_INT_ACK_CALLBACK      1

/*
 * Enable this if you need/change PC, flags or cycles in your IrqCallback function.
 */
#define INT_ACK_NEEDS_STUFF       0
#define INT_ACK_CHANGES_STUFF     0

/*
 * If enabled, ResetCallback is called from the context, whenever RESET opcode is
 * encountered. All context members are valid and can be changed.
 * If disabled, RESET opcode acts as an NOP.
 */
#define USE_RESET_CALLBACK        1

/*
 * If enabled, UnrecognizedCallback is called if an invalid opcode is
 * encountered. All context members are valid and can be changed. The handler
 * should return zero if you want Cyclone to gererate "Illegal Instruction"
 * exception after this, or nonzero if not. In the later case you should change
 * the PC by yourself, or else Cyclone will keep executing that opcode all over
 * again.
 * If disabled, "Illegal Instruction" exception is generated and execution is
 * continued.
 */
#define USE_UNRECOGNIZED_CALLBACK 1

/*
 * This option will also call UnrecognizedCallback for a-line and f-line
 * (0xa*** and 0xf***) opcodes the same way as described above, only appropriate
 * exceptions will be generated.
 */
#define USE_AFLINE_CALLBACK       1

/*
 * This makes Cyclone to call checkpc from it's context whenever it changes the PC
 * by a large value. It takes and should return the PC value in PC+membase form.
 * The flags and cycle counter are not valid in this function.
 */
#define USE_CHECKPC_CALLBACK      1

/*
 * When this option is enabled Cyclone will do two word writes instead of one
 * long write when handling MOVE.L with pre-decrementing destination, as described in
 * Bart Trzynadlowski's doc (http://www.trzy.org/files/68knotes.txt).
 * Enable this if you are emulating a 16 bit system.
 */
#define SPLIT_MOVEL_PD            1
