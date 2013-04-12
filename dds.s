	NAME dds
	
	PUBLIC dds_init
	PUBLIC load_new_segment
	PUBLIC prepare
	
	EXTERN dac
	EXTERN preheat
	EXTERN lookup
	
	SECTION `.tiny.bss`:DATA:REORDER:NOROOT(0) 
voltage:
	DS8 2
	SECTION `.tiny.bss`:DATA:REORDER:NOROOT(0) 
new_seg_ptr:
	DS8 2
	SECTION `.tiny.bss`:DATA:REORDER:NOROOT(0) 
leap_score:
	DS8 2
	SECTION `.tiny.bss`:DATA:REORDER:NOROOT(0) 
points_in_segment:
	DS8 2
	SECTION `.tiny.bss`:DATA:REORDER:NOROOT(0) 
step:
	DS8 2
	SECTION `.tiny.bss`:DATA:REORDER:NOROOT(0) 
leap_dec:
	DS8 2
	SECTION `.tiny.bss`:DATA:REORDER:NOROOT(0) 
leap_inc:
	DS8 2
	SECTION `.tiny.bss`:DATA:REORDER:NOROOT(0) 
flags:
	DS8 1
	SECTION `.far_func.text`:CODE:REORDER:NOROOT(0)
	CODE

dds_init:
	LDW	Y, preheat
	LDW	voltage, Y
	CALL	load_new_segment
	RETF
	
	SECTION `.far_func.text`:CODE:REORDER:NOROOT(0)
load_new_segment:
	LDW	Y, X
	LDW	Y, (Y)
	LDW	points_in_segment, Y
	ADDW	X, #2
	LDW	Y, X
	LDW	Y, (Y)
	LDW	step, Y
	ADDW	X, #2
	LDW	Y, X
	LDW	Y, (Y)
	LDW	leap_dec, Y
	ADDW	X, #2
	LDW	Y, X
	LDW	Y, (Y)
	LDW	leap_inc, Y
	ADDW	X, #2
	LD	A, (X)
	LD	flags, A
	INCW	X
	LDW	new_seg_ptr, X
	CLRW	Y
	LDW	leap_score, Y
	RET
prepare:
//	LDW	X, #dac
	LDW	Y, X  // в Х лежит адрес начала буфера
	ADDW	Y, #100   // в стековой переменной адрес последнего элемента буфера
	PUSHW	Y
loop:
	LDW	Y, points_in_segment   
	JRNE	continue          
	// сегмент закончился
	BTJT	flags, #0, loop_zero	
	// не последний сегмент
	PUSHW	X
	LDW	X, new_seg_ptr
	CALL	load_new_segment
	POPW	X
	LDW	Y, points_in_segment
continue:
	DECW	Y
	LDW	points_in_segment, Y	// уменьшаем счетчик оставшихся точек на 1
	LDW	Y, leap_score		// проверяем точку на високосность
	JRMI	leap
	//	точка невисокосная
	SUBW	Y, leap_dec
	LDW	leap_score, Y
	CLRW	Y
	JRA	leap_endif
leap:
	//	точка високосная
	ADDW	Y, leap_inc
	LDW	leap_score, Y
	LDW	Y, #1
leap_endif:
	ADDW	Y, step
	ADDW	Y, voltage
	LDW	voltage, Y
	SLLW	Y
	LDF	A, (lookup, Y)
	LD	(X), A
	LDF	A, (lookup+1, Y)
	LD	(1, X), A
//	LDW	(X), Y
	ADDW	X, #2
	CPW	X, (1,SP)
	JRNE	loop
	ADD	SP, #2
	RETF
loop_zero:
	CLRW	Y
	LDW	(X), Y
	ADDW	X, #2
	CPW	X, (1,SP)
	JRNE	loop_zero
	ADD	SP, #2
	RETF
	END