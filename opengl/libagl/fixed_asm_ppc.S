/*
** Copyright 2012, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License"); 
** you may not use this file except in compliance with the License. 
** You may obtain a copy of the License at 
**
**     http://www.apache.org/licenses/LICENSE-2.0 
**
** Unless required by applicable law or agreed to in writing, software 
** distributed under the License is distributed on an "AS IS" BASIS, 
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
** See the License for the specific language governing permissions and 
** limitations under the License.
*/

	.global gglFloatToFixed
	.type	gglFloatToFixed, @function
gglFloatToFixed:
	stwu r1, -16(r1)
	stfs f1, 8(r1)
	lwz r0,8(r1)

	rlwinm r6, r0, 9,24,31 # exponent
	cmpwi r6, 110
	bgt+ .Lnzero
	li r3, 0
	addi r1, r1, 16
	blr

.Lnzero:
	addic. r6, r6, -(127+15)
	bge- .Loverflow # +overflow or -overflow

	neg r6, r6 # shift

	slwi r9, r0, 8 # mantissa << 8
	oris r9, r9, 0x8000 # add missing 1.

	# round to nearest, by adding .5

	li r4, 1
	addi r5, r6, -1
	slw r5, r4, r5
	and. r4, r5, r9
	srw r3, r9, r6
	beq 0f
	addi r3, r3, 1
0:

	cmpwi r0, 0 # add sign
	bge 2f
	neg r3, r3 

2:
	addi r1, r1, 16
	blr


.Loverflow:
	cmpwi r0, 0 # -overflow ?
	lis r3, 0x7fff
	ori r3, r3, 0xffff
	addi r1, r1, 16
	blr

.Lnegoverflow:
	lis r3, 0x8000 # 0x80000000 if it's zero

	addi r1, r1, 16
	blr
