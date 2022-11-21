#
# Copyright (C) 2022 Linzhi Ltd.
#
# This work is licensed under the terms of the MIT License.
# A copy of the license can be found in the file COPYING.txt
#

favicon.ico:	Makefile
		# \345\207\244 is 0xe5 0x87 0xa4, the UTF-8 encoding of
		# https://en.wiktionary.org/wiki/%e5%87%a4
		# \342\234\224 is U+2714
		# https://en.wikipedia.org/wiki/Dingbat
		convert -size 320x320 -fill '#88a6e0' \
                    -gravity center \
                    -font WenQuanYi-Micro-Hei \
                    label:"`echo '\345\207\244'`" \
                    -background None \
                    -fill '#008000' \
                    -font Dingbats \
                    label:"`echo '\342\234\224'`" \
                    -layers flatten \
                    -trim -scale '32x32!' "$@"
