#!/usr/bin/env python
# coding=UTF-8
import sys

def mangle_label(label):
	return label.replace(" ", "_")

out = sys.stdout
linenum = 0
chunk = None
codebegin = False
anchors = set()
last_defn = None
last_defn_add = False
for line in sys.stdin:
	line = line[:-1]
	linenum = linenum + 1
	if len(line) == 0 or line[0] != '@':
		sys.stderr.write("%s: Line too short or does not start with @\n" % linenum)
		continue
	p = line.partition(" ")
	keyword = p[0]
	arg = p[2]

	if keyword == "@text":
		out.write(arg)
	elif keyword == "@nl":
		out.write("\n")
		if codebegin:
			codebegin = False
			language="c"
			if last_defn.endswith(".s"):
				language="gas"
			elif last_defn.endswith(".simple"):
				language="none"
			out.write(".. highlight:: %s\n\n" % language)
			out.write("``⟨%s⟩%s≡`` ::\n" % (last_defn, "+" if last_defn_add else ""))
			out.write("\t\n")
			out.write("\t")
		elif chunk == "code":
			out.write("\t")
	elif keyword == "@file":
		continue
	elif keyword == "@defn":
		if arg not in anchors:
			out.write(".. _%s:\n" % mangle_label(arg))
			anchors.add(arg)
			last_defn_add = False
		else:
			last_defn_add = True
		last_defn = arg
		continue
	elif keyword == "@quote" or keyword== "@endquote":
		out.write("``")
	elif keyword == "@use":
		#out.write("<<:ref:%s <%s>>>" % (arg, mangle_label(arg)))
		out.write("<<%s>>" % arg)
	elif keyword == "@begin":
		s = arg.split()
		chunk = s[0]
		if chunk == "code":
			codebegin = True
	elif keyword == "@end":
		s = arg.split()
		endchunk = s[0]
		if endchunk != chunk:
			sys.stderr.write("%s: Mismatched endchunk (%s, expected %s)\n" % (linenum, endchunk, chunk))
		if endchunk == "docs":
			out.write("\n")
		chunk = None
