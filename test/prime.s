	add	$s1, $zero, $zero	# cnt = 0;
	addi	$s0, $zero, 100		# n = 100;

	addi	$s2, $zero, 2		# i = 2;
L0:	slt	$t0, $s2, $s0		# while (i < n) {
	beq	$t0, $zero, L3
	sll	$t0, $s2, 2		# 	if (!a[i]) {
	lw	$t1, 0($t0)	
	bne	$t1, $zero, L2
	addi	$s1, $s1, 1		# 		cnt++

	add	$s3, $s2, $s2		# 		j = i+i
L1:	slt	$t0, $s3, $s0		# 		while (j < n) {
	beq	$t0, $zero, L2
	addi	$t0, $zero, 1		# 			a[j] = 1
	sll	$t1, $s3, 2
	sw	$t0, 0($t1)
	add	$s3, $s3, $s2		# 			j = j+i
	j	L1			# 		}
					#	}
L2:	addi	$s2, $s2, 1		# 	i++;
	j	L0			# }
L3:	nop

