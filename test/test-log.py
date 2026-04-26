#!/usr/bin/env python3

with open("nestest.log", "r") as f:
	oldlog = f.read().strip().split("\n")

goodlog = []

for line in oldlog:
	"""
C000  4C F5 C5  JMP $C5F5                       A:00 X:00 Y:00 P:24 SP:FD PPU:  0, 21 CYC:7
C000 4C F5 C5 JMP a:00 x:00 y:00 p:00100100 sp:FD cyc:7
	"""
	newl = (
		line[0:4] + " " +
		line[6:14] + " " +
		line[16:19] + " " +
		"a:" + line[50:52] + " " +
		"x:" + line[55:57] + " " +
		"y:" + line[60:62] + " " +
		"p:" + f"{int(line[65:67], 16):08b}" + " " +
		"sp:" + line[71:73] + " " +
		# "ppu:" + line[78:85] + " " +
		"cyc:" + line[90:]
		
	)
	
	goodlog.append(newl)


with open("cpu.log", "r") as f:
	cpulog = f.read().strip().split("\n")

error = 0

for i in range(max(len(goodlog), len(cpulog))):
	# for up to ten errors, print both log lines
	# on first error, print 5 preceding lines before
	
	if i < len(goodlog):
		goodline = goodlog[i]
	else:
		goodline = " " * 40
	
	if i < len(cpulog):
		cpuline = cpulog[i]
	else:
		cpuline = " " * 40
	
	if goodline != cpuline:
		if error == 0:
			print("First error at line", i)
			print("Showing 5 lines before:")
			for j in range(max(0, i-5), i):
				if j < len(goodlog):
					print(f"  {goodlog[j]}")
				else:
					print(" " * 40)
				if j < len(cpulog):
					print(f"  {cpulog[j]}")
				else:
					print(" " * 40)
			print("End of preceding lines.")
		error += 1
		print(f"Error at line {i}:")
		print(f"  Expected: {goodline}")
		print(f"  Got:      {cpuline}")
	
	
	
	if error > 10:
		print("Too many errors, stopping.")
		break