#!/usr/bin/python

import sys
import math

import scipy
import scipy.linalg

# usage
# pp.py input_file
#
# output file name is automaticaly created


# sample name
name = []
# sample cls
cls = []
#[] takes count for hit when the sample in oob
oob_hit = []
#[] takes count for the sample in oob
oob_count = []
#proximity matrix [][]
prox_mat = []
#oob oriented proximity matrix [][]
oob_prox_mat = []
# +1 when the sample judge is missed at the time of the attr is permutated
# sample attribute interaction matrix (sai mat)
sai_mat = []
# sample length
sample_len = 0


# for outlier calculation
def outlier( ):
    cls_size = {}
    outlier_vals = []

    for v in cls:
        _cnt = cls_size.setdefault( v , 0 )
        _cnt += 1
        cls_size[v] = _cnt

    outlier_vals = [0] * sample_len
    for i in range( sample_len ):
        for j in range( sample_len ):
            if i != j and cls[i] == cls[j]:
                outlier_vals[i] += prox_mat[i][j] * prox_mat[i][j]
    for i in range( sample_len ):
        _temp = outlier_vals[i]
        outlier_vals[i] = cls_size[cls[i]] / _temp

    return outlier_vals

# for proximity matrix projection
# you can choose pure prox or oob prox
def prox_mat_projection(mat):
    cv = []
    #init cv
    for i in range( sample_len ):
        cv.append( [0] * sample_len )

    m_prox_mat = scipy.matrix( mat )
    for i in range( sample_len ):
        for j in range( sample_len ):
            cv[i][j] = 0.5*( m_prox_mat[i,j] - m_prox_mat[i].mean() \
                             - m_prox_mat[:][:,j].mean() + m_prox_mat.mean() )
    m_cv = scipy.matrix( cv )
    la,vec = scipy.linalg.eig( m_cv )
    m_vec = scipy.matrix( vec )
    x = [ la[0] * v for v in  m_vec[:][:,0].flatten().tolist()[0]]
    y = [ la[1] * v for v in  m_vec[:][:,1].flatten().tolist()[0]]
    z = [ la[2] * v for v in  m_vec[:][:,2].flatten().tolist()[0]]
    return [ float(v) for v in x],[ float(v) for v in y],[ float(v) for v in z]
    #return x,y

def output_sample_info(file_name):
    of = open( file_name ,'w' )
    of.writelines( 'name\tcls\toob_hit\toob_count\t1.0-oobee\toutlier\tprox_x\tprox_y\tprox_z\toob_prox_x\toob_prox_y\toob_prox_z\n' )
    for i in range( sample_len ):
        p = float(oob_hit[i]) / float( oob_count[i] ) if int(oob_count[i]) != 0 else -1.0
        of.writelines( name[i] + '\t' + str(cls[i]) + '\t'
                      + str(oob_hit[i]) + '\t' \
                      + str(oob_count[i]) + '\t' + str( p ) + '\t' \
                      + str(outlier_vals[i]) + '\t' \
                      + str(prox_x[i]) + '\t' + str(prox_y[i]) + '\t' + str(prox_z[i]) + '\t' \
                      + str(oob_prox_x[i]) + '\t' + str(oob_prox_y[i]) + '\t' + str(oob_prox_z[i]) + '\n') 
    of.flush()
    of.close()


# command [input data(from C++ out)] [output file]

if __name__ == '__main__':


 
    f = open( sys.argv[1] , 'r' )
    name = f.readline().strip().split('\t')[1:]
    cls = f.readline().strip().split('\t')[1:]
    oob_hit = f.readline().strip().split('\t')[1:]
    oob_count = f.readline().strip().split('\t')[1:]

    #prox_mat
    f.readline(); # for header
    sample_len = len( name )
    for i in range( sample_len ):
        prox_mat.append( [float(v) for v in f.readline().strip().split('\t')] )

    #oob_prox_mat
    f.readline(); # for header
    for i in range( sample_len ):
        oob_prox_mat.append( [float(v) for v in f.readline().strip().split('\t')] )

    #sai_mat must be in tail of this file ( because I don't know attribute length )
    f.readline(); # fro header
    for l in f:
        sai_mat.append( l.strip().split('\t') )

    f.close()

    outlier_vals = outlier()
    prox_x,prox_y,prox_z = prox_mat_projection(prox_mat)
    oob_prox_x,oob_prox_y,oob_prox_z = prox_mat_projection(oob_prox_mat)

    out_file_name = sys.argv[1].replace('.txt','_table.txt')
    output_sample_info( out_file_name )
    

