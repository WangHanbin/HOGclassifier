#include "svm_binder.h"
#include "traversability_analyzer.h"
#include <ctime>

void SvmBinder::setProblemTest(vector<vector<float> >* hogs)
{
	if (!hogs->size())
	{
		cerr<<"hogs are empty"<<endl;
		return;
	}
	//assuming that each patch is of same size
	int problemSize = hogs->size();
	int valueVectorSize = hogs->at(0).size();

	
	struct svm_node *x_space;

	_probTest.l = problemSize;
	// _probTest.y = new double[problemSize];
	_probTest.y = Malloc(double,problemSize); //space for prob.l doubles
	// _probTest.x = new svm_node*[problemSize];
	_probTest.x = Malloc(struct svm_node *, problemSize); //space for prob.l pointers to struct svm_node
	// x_space = new svm_node[(valueVectorSize+1) * problemSize];
	x_space = Malloc(struct svm_node, (valueVectorSize+1) * problemSize); //memory for pairs of index/value

	//initialize the different lables with an array of labels
	for (int i=0; i < problemSize; ++i)
	{
		_probTest.y[i] = 0;
	}
	//initialize the svm_node vector with input data array as follows:
	int j=0; //counter to traverse x_space[i];
	for (int i=0;i < problemSize; ++i)
	{
		//set i-th element of prob.x to the address of x_space[j]. 
		//elements from x_space[j] to x_space[j+data[i].size] get filled right after next line
		_probTest.x[i] = &x_space[j];
		for (int k=0; k<valueVectorSize; ++k, ++j)
		{
			x_space[j].index=k+1; //index of value
			x_space[j].value=hogs->at(i)[k]; //value
		}
		x_space[j].index=-1;//state the end of data vector
		x_space[j].value=0;
		j++;
	}
}

void SvmBinder::setProblemTrain(vector<vector<float> >* hogsPos, vector<vector<float> >* hogsNeg)
{
	if (!hogsPos->size() || !hogsNeg->size())
	{
		cerr<<"hogs are empty"<<endl;
		return;
	}
	int problemSize = hogsPos->size() + hogsNeg->size();
	//assuming all hogs have same size
	int valueVectorSize = hogsPos->at(0).size();
	
	struct svm_node *x_space;

	_probTrain.l = problemSize;
	_probTrain.y = Malloc(double,problemSize); //space for prob.l doubles
	_probTrain.x = Malloc(struct svm_node *, problemSize); //space for prob.l pointers to struct svm_node
	x_space = Malloc(struct svm_node, (valueVectorSize+1) * problemSize); //memory for pairs of index/value

	//initialize the different lables with an array of labels
	int i=0;
	while (i<problemSize)
	{
		if (i<hogsPos->size())
			_probTrain.y[i] = 1;
		else
			_probTrain.y[i] = -1;
		i++;
	}
	
	//initialize the svm_node vector with input data array as follows:
	int j=0; //counter to traverse x_space[i];
	for (int i=0; i < hogsPos->size(); ++i)
	{
		//set i-th element of prob.x to the address of x_space[j]. 
		//elements from x_space[j] to x_space[j+data[i].size] get filled right after next line
		_probTrain.x[i] = &x_space[j];
		for (int k=0; k<valueVectorSize; ++k, ++j)
		{
			x_space[j].index=k+1; //index of value
			x_space[j].value=hogsPos->at(i)[k]; //value
		}
		x_space[j].index=-1;//state the end of data vector
		x_space[j].value=0;
		j++;
	}
	for (int i=0;i < hogsNeg->size(); ++i)
	{
		//set i-th element of prob.x to the address of x_space[j]. 
		//elements from x_space[j] to x_space[j+data[i].size] get filled right after next line
		_probTrain.x[i] = &x_space[j];
		for (int k=0; k<valueVectorSize; ++k, ++j)
		{
			x_space[j].index=k+1; //index of value
			x_space[j].value=hogsNeg->at(i)[k]; //value
		}
		x_space[j].index=-1;//state the end of data vector
		x_space[j].value=0;
		j++;
	}
}


SvmBinder::SvmBinder()
{
	_model = NULL;
	//setting all the defaults
	_param.svm_type = C_SVC;
	_param.kernel_type = LINEAR;
	_param.degree = 3;
	_param.gamma = 1;	// 1/num_features
	_param.coef0 = 0;
	_param.nu = 0.5;
	_param.cache_size = 100;
	_param.C = 32;
	_param.eps = 1e-3;
	_param.p = 0.1;
	_param.shrinking = 1;
	_param.probability = 0;
	_param.nr_weight = 0;
	_param.weight_label = NULL;
	_param.weight = NULL;
}
SvmBinder::~SvmBinder()
{
	svm_free_and_destroy_model(&_model);
	_model = NULL;
}

bool SvmBinder::loadModel()
{
	_model = svm_load_model(_modelPath.c_str());
	if (!_model)
	{
		Logger::instance()->logInfo("no model provided");
		return false;
	}
	Logger::instance()->logInfo("model is loaded");
	return true;
}

bool SvmBinder::hasModelPath()
{
	if (_modelPath.empty())
		return false;
	return true;
}

string& SvmBinder::modelPath()
{
	return _modelPath;
}

void SvmBinder::train(vector<vector<float> >* hogsPos, vector<vector<float> >* hogsNeg)
{
	Logger::instance()->logInfo("training model...");
	setProblemTrain(hogsPos, hogsNeg);
	_model = svm_train(&_probTrain, &_param);
}
vector<int>* SvmBinder::test(vector<vector<float> >* hogs)
{
	if (!_model)
		return NULL;
	setProblemTest(hogs);
	_newLabels.resize(hogs->size());
	cerr<<"hogs size = "<<hogs->size()<<endl;
	#pragma omp parallel for
	for (uint i = 0; i < hogs->size(); ++i)
	{
		if (i%1000==0)
			cerr<<".";
		_newLabels[i] = svm_predict(_model, _probTest.x[i]);
	}
	cerr<<endl;
	return &_newLabels;

}
void SvmBinder::saveModel()
{
	svm_save_model(_modelPath.c_str(), _model);
	Logger::instance()->logInfo("model was saved to", _modelPath);
}

//quite specific for our problem.
//far from being general
void SvmBinder::createHyperPlane()
{
	std::vector <double> coeffs;
	std::vector  < std::vector <double> > supportVectors;
	
	int nr_sv =0;
	_bias =0;

	_versor = _model->label[0];
	for (int i = 0; i < _model->nr_class; ++i)
	{
		nr_sv += _model->nSV[i];
	}
	coeffs = std::vector <double> (nr_sv, 0);

	_bias = _model->rho[0];
	supportVectors = std::vector  < std::vector <double> > (nr_sv);
	for (int i = 0; i<_model->l; ++i)
	{
		coeffs[i] = _model->sv_coef[0][i];

		int j=0;
		while (_model->SV[i][j].index != -1)
		{
			supportVectors[i].push_back(_model->SV[i][j].value);
			j++;
		}
	}

	_hyperPlane = std::vector <double > (supportVectors[0].size(),0);
	for (uint i=0;i<supportVectors.size();i++)
		for (uint j=0;j<supportVectors[i].size();j++)
			_hyperPlane[j] += coeffs[i] * supportVectors[i][j];

	Logger::instance()->logVerticalSpace();
	Logger::instance()->logInfo("MODEL PARAMETERS");
	Logger::instance()->logInfo("svm_type:", _model->param.svm_type);
	Logger::instance()->logInfo("kernel_type:",_model->param.kernel_type);
	Logger::instance()->logInfo("nr_class:",_model->nr_class);
	Logger::instance()->logInfo("total_sv:",nr_sv);
	Logger::instance()->logInfo("rho:",_bias);
	Logger::instance()->logInfo("label:",_model->label[0], _model->label[1]);
	Logger::instance()->logInfo("nr_sv:",_model->nSV[0], _model->nSV[1]);


	Logger::instance()->logInfo("HYPERPLANE");
	for (size_t i=0; i< _hyperPlane.size(); ++i)
	{
		cout<< _hyperPlane[i]<<" ";
	}
	cout<<endl;
}

vector<int>* SvmBinder::testWithHyperPlane(vector<vector<float> >* hogs)
{
	double dpr=0;
	_newLabels.resize(hogs->size());
	for (uint i = 0; i<hogs->size(); ++i)
	{
		dpr = 0;
		for (uint j=0; j<hogs->at(i).size(); j++)
			dpr += hogs->at(i)[j] * _hyperPlane[j];
		dpr = (dpr - _bias)*(double)(_versor);

		if(dpr > 0)
			_newLabels[i] = TRAVERSABLE;
		else
			_newLabels[i] = NON_TRAVERSABLE;
	}
	return &_newLabels;
}
