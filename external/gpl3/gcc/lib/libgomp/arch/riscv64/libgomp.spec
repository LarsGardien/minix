# This file is automatically generated.  DO NOT EDIT!
# Generated from: NetBSD: mknative-gcc,v 1.79 2014/05/29 16:27:50 skrll Exp 
# Generated from: NetBSD: mknative.common,v 1.11 2014/02/17 21:39:43 christos Exp 
#
# This spec file is read by gcc when linking.  It is used to specify the
# standard libraries we need in order to link with -fopenmp.
*link_gomp: -lgomp %{static: }