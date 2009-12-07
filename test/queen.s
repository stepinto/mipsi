	add   $gp, $zero, $zero
	addi  $sp, $zero, 10000
main:
	addi  $s1, $zero, 0
	addi  $s2, $zero, 8	     
	add   $s2, $zero, $v0
	addi  $a0, $zero, 0         
	add   $a1, $zero, $s2       
	add   $a2, $zero, $s1       
	jal   Queen              
	add   $s0, $zero, $v0		     
	add   $a0, $zero, $s0
	j     Exit

Queen:
	addi  $sp, $sp, -32
	sw    $v0, 28($sp)
	sw    $ra, 24($sp)       
	sw    $a0, 20($sp)
	sw    $a1, 16($sp)
	sw    $a2, 12($sp)
	sw    $s5, 8($sp)
	sw    $s6, 4($sp)

	addi  $t0, $zero, 1         
	sw    $t0, 0($sp)		 
	addi  $t1, $a1, 1        
	
	bne   $a0, $a1, Loop	 
	addi  $a2, $a2, 1        
	sw    $a2, 12($sp)        
	j     Exit1
	
Loop:
	slt   $t2, $t0, $t1      
	beq   $t2, $zero,  Exit1    
	lw    $a0, 20($sp)		 
        add $t0, $t0, $t0
        add $t5, $t0, $t0
	sub   $s6, $gp, $s5 
	sw    $t0, 0($s6)	     
	
	jal   Valid		         
	beq   $v1, $zero, FLAG	     
	addi  $a0, $a0, 1 	     
	jal   Queen
	add   $a2, $zero, $v0  	     
	sw    $a2, 12($sp)
	
FLAG:
	lw    $t0, 0($sp)		 
	addi  $t0, $t0, 1	     
	sw    $t0, 0($sp)		 
	j     Loop

Exit1:
	lw    $ra, 24($sp)	     
	lw	  $a2, 12($sp)
	add   $v0, $zero, $a2  	     
	lw    $s5, 8($sp)
	lw    $s6, 4($sp)
	lw    $t0, 0($sp)		 
	addi  $sp, $sp, 32	     
				             
	jr    $ra                
Valid: 
	addi  $sp, $sp, -12
        sw    $s5, 8($sp)
	sw    $s6, 4($sp)
	sw    $s7, 0($sp)
   
	addi  $t3, $zero,  0        
	
Lp:
	slt   $t5, $t3, $a0	     
	beq   $t5, $zero,  Exit2    
	
        add $t3, $t3, $t3
        add $t5, $t3, $t3
	sub   $s6, $gp, $s5
	lw    $t4, 0($s6)	     
	
	beq   $t4, $t0, Exit3	 
	sub   $t6, $t4, $t0	     
	sub   $t7, $a0, $t3	     
	sub   $s7, $t3, $a0	     
	beq   $t6, $t7, Exit3	 
	beq   $t6, $s7, Exit3	 
	addi  $t3, $t3, 1	     
	j	  Lp			     

Exit2:
	lw    $s5, 8($sp)
	lw    $s6, 4($sp)
	lw    $s7, 0($sp)
	addi  $sp, $sp, 12
	addi  $v1, $zero, 1         
	jr    $ra
	     
Exit3:
	lw    $s5, 8($sp)
	lw    $s6, 4($sp)
	lw    $s7, 0($sp)
	addi  $sp, $sp, 12
	addi  $v1, $zero, 0         
	jr    $ra
Exit:
	nop

