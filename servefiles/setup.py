#!/usr/bin/env python
# coding: utf-8 -*-

from setuptools import setup

setup(
	name="servefiles",
	version="2.4.12",
	scripts=["servefiles.py", "sendurls.py"],
	py_modules=["i18n", "lang_en", "lang_zh_cn"],
	author="Steveice10",
	author_email="Steveice10@gmail.com",
	description="Simple Python scripts for sending local files to the FBI remote installer.",
	license="MIT",
	url="https://github.com/nicedayzhu/FBI/tree/master/servefiles"
)
