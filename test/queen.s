	# we need 3 arrays: a[0..7], b[-7..7], c[0..14]
	add	$s0, $zero, $zero
	addi	$s1, $zero, 100
	addi	$s2, $zero, 200
	addi	$sp, $zero, 65536

	add	$s3, $zero, $zero	# cnt = 0
	addi	$s4, $zero, 8		# n = 0

	add	$a0, $zero, $zero
	jal	DFS
	jal	END

DFS:			# dfs(int dep):
	addi	$sp, $sp, -40
	sw	$ra, 0($sp)
	sw	$s5, -4($sp)
	sw	$t0, -8($sp)
	sw	$t1, -12($sp)
	sw	$t2, -16($sp)
	sw	$t3, -20($sp)
	sw	$t4, -24($sp)
	sw	$t5, -28($sp)
	sw	$t6, -32($sp)
	sw	$t7, -36($sp)

	bne	$a0, $s4, L0		# if (dep == n) {
	add	$s3, $s3, 1		#	cnt++;
	j	L2			#	return;
					# }
L0:	add	$s5, $zero, $zero	# i = 0
L1:	slt	$t0, $s5, $s4		# while (i < n) {
	beq	$t0, $zero, L3

	sll	$t1, $s5, 2		#	t1 = &a[i];
	add	$t1, $t1, $s1
	sub	$t2, $a0, $s5		#	t2 = &b[dep-i];
	sll	$t2, $t2, 2
	add	$t2, $t2, $s2
	add	$t3, $a0, $s5		#	t3 = &c[dep+i];
	add	$t3, $t3, $s3
	sll	$t3, $t3, 2
	
	lw	$t4, 0($t1)		#	t4 = !(*t1);
	nor	$t4, $t4, $t4
	lw	$t5, 0($t2)		#	t5 = !(*t2);
	nor	$t5, $t5, $t5
	lw	$t6, 0($t3)		#	t6 = !(*t3);
	nor	$t6, $t6, $t6		#	if (t4 && t5 && t6) {
	and	$t7, $t4, $t5
	and	$t7, $t7, $t6
	beq	$t7, $zero, L2
	addi	$t0, $zero, 1
	sw	$t0, 0($t1)		#		*t1 = 1;
	sw	$t0, 0($t2)		#		*t2 = 1;
	sw	$t0, 0($t3)		#		*t3 = 1;
	
	addi	$a0, $a0, 1		#		dfs(dep + 1);
	jal	DFS
	addi	$a0, $a0, -1

	sw	$zero, 0($t1)		#		*t1 = 0;
	sw	$zero, 0($t2)		#		*t2 = 0;
	sw	$zero, 0($t3)		#		*t3 = 0;
					#	}

L2:	addi	$s5, $s5, 1
	j	L1			# }

L3:	lw	$at, 0($sp)		# return
	lw	$ra, 0($sp)
	lw	$s5, 4($sp)
	lw	$t0, 8($sp)
	lw	$t1, 12($sp)
	lw	$t2, 16($sp)
	lw	$t3, 20($sp)
	lw	$t4, 24($sp)
	lw	$t5, 28($sp)
	lw	$t6, 32($sp)
	lw	$t7, 36($sp)
	addi	$sp, $sp, 40
	jr	$at
	
END:	nop

