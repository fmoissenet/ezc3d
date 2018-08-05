#include "mex.h"
#include <iostream>
#include <memory>

#include "ezc3d.h"

// From values to matlab
void fillMatlabField(mxArray *field, mwIndex idx, int value);
void fillMatlabField(mxArray *field, mwIndex idx, const std::vector<int>& values);

void fillMatlabField(mxArray *field, mwIndex idx, double value);
void fillMatlabField(mxArray *field, mwIndex idx, const std::vector<float>& values);
void fillMatlabField(mxArray *field, mwIndex idx, const std::vector<double>& values);

void fillMatlabField(mxArray *field, mwIndex idx, const std::string &value);
void fillMatlabField(mxArray *field, mwIndex idx, const std::vector<std::string>& values);

// From matlab to values

int toInteger(const mxArray * prhs);
double toDouble(const mxArray * prhs);
bool toBool(const mxArray * prhs);
std::string toString(const mxArray * prhs);
