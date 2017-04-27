#include <iostream>
#include "json/json.h"
#include <fstream>
#include <vector>
#include <map>
#include <algorithm>
#include <cctype>

using namespace std;

class interstoryParam {
public:
    int story;
    double K0,Sy,eta,C,gamma,eta_soft,alpha,beta,a_k,omega;
};
class floorParam {
public:
    int floor;
    double mass;
};
double lambda(int n) {
    return 0.4053*(double)(n*n)+0.405*(double)(n)+0.1869;
}

class HazusData {
public:
    int lowlmt,highlmt;		//low story, high story
    double Props[10];		//10-parameter hysteretic model.
    double damage[4];		//damage criteria
    double T1,T2,damp;		//T1=T0/N  (T0:foundamental period, N: number of stories)，T2=0.333
    double hos;				//story height
};

Json::Value GenerateBuildingModel(string path);


int main()
{
    string s="MDOF-shear.json";
    Json::Value bldg=GenerateBuildingModel(s);

    return 0;
}


Json::Value GenerateBuildingModel(string path)
{
    string strucType="";
    int year=0;
    double s_height = 0;
    int noStories= 0;
    double area=0;

    //Parse Json input file
    Json::Reader reader;
    Json::Value root;
    ifstream fin(path, ios::binary);
    if( !fin.is_open() )
    {
        cout << "Error opening file "<<path.c_str()<<"\n";
        exit(0);
    }

    if(reader.parse(fin,root))
    {
        strucType=root["BuildingDescription"]["strucType"].asString();
        transform(strucType.begin(), strucType.end(), strucType.begin(), ::toupper);
        year=root["BuildingDescription"]["year"].asInt();
        s_height=root["BuildingDescription"]["s_height"].asDouble();
        noStories=root["BuildingDescription"]["noStories"].asInt();
        area=root["BuildingDescription"]["area"].asDouble();

    }
    else
    {
        cout << "Error parsing file "<<path.c_str()<<"\n" << endl;
    }
    fin.close();

    //Load Hazus database
    const double PI=3.14159265358979;
    const double UNIT_MASS=1000.0;
    const int nCodeLevel=4;
    const int nBldgType=36;
    //HazusData hazus[nCodeLevel][nBldgType];
    map<string, HazusData> hazus[nCodeLevel];
    string temps="";
    ifstream fHazus("HazusData.txt");
    if( !fHazus.is_open() )
    {
        cout << "Error opening file HazusData.txt!\n";
        exit(0);
    }
    for (int i=0;i<nCodeLevel;++i)
    {
        getline(fHazus,temps);
        for(int j=0;j<nBldgType;++j)
        {
            string type="";
            HazusData hd;
            fHazus>>temps>>type>>hd.lowlmt>>hd.highlmt;
            //fHazus>>temps>>hazus[i][j].name>>hazus[i][j].lowlmt>>hazus[i][j].highlmt;
            for (int k=0;k<10;k++)
            {
                fHazus>>hd.Props[k];
            }
            for (int k=0;k<4;k++)
            {
                fHazus>>hd.damage[k];
            }
            fHazus>>hd.T1>>hd.T2>>hd.hos>>hd.damp;
            hazus[i].insert(pair<string,HazusData>(type,hd));
        }
    }
    fHazus.close();


    //Calculate parameters of MDOF models
    string type= "MDOF_shear_model";
    double dampingRatio = 0.0;
    double damageCriteria[4] = {0,0,0,0};
    vector <interstoryParam> interstoryParams;
    vector <floorParam> floorParams;
    interstoryParams.clear();
    floorParams.clear();
    interstoryParams.resize(noStories);
    floorParams.resize(noStories);

    //Determine seismic design level
    // (Need further improvement: does not include low-code, does not consider seismic zone)
    int codelevel=0;
    if(1973<year)
        codelevel=0;    //high-code
    else if (1941<=year && year<=1973)
        codelevel=1;    //moderate-code
    else
        codelevel=3;    //pre-code

    dampingRatio=hazus[codelevel][strucType].damp;
    for (int i=0;i<4;i++)
    {
        damageCriteria[i]=hazus[codelevel][strucType].damage[i];
    }
    double T0=noStories*hazus[codelevel][strucType].T1;
    for (int i=0;i<noStories;++i)
    {
        floorParams[i].floor=i+1;
        floorParams[i].mass=area*UNIT_MASS;
        interstoryParams[i].story=i+1;
        interstoryParams[i].K0=4.0*PI*PI*lambda(noStories-1)*floorParams[i].mass/T0/T0;

        double r = 1.0-double(i+1)*double(i)/double(noStories)/double(noStories+1);
        interstoryParams[i].Sy=hazus[codelevel][strucType].Props[1]*floorParams[i].mass*9.8*double(noStories)*r;
        interstoryParams[i].eta=hazus[codelevel][strucType].Props[2];
        interstoryParams[i].C=hazus[codelevel][strucType].Props[3];
        interstoryParams[i].gamma=hazus[codelevel][strucType].Props[4];
        interstoryParams[i].eta_soft=hazus[codelevel][strucType].Props[5];
        interstoryParams[i].alpha=hazus[codelevel][strucType].Props[6];
        interstoryParams[i].beta=hazus[codelevel][strucType].Props[7];
        interstoryParams[i].a_k=hazus[codelevel][strucType].Props[8];
        interstoryParams[i].omega=hazus[codelevel][strucType].Props[9];
    }


    //output
    Json::Value modelData;
    modelData["dampingRatio"]=Json::Value(dampingRatio);
    for(int i=0;i<4;++i)
        modelData["damageCriteria"].append(damageCriteria[i]);

    for(int i=0;i<noStories;++i)
    {
        Json::Value flp;
        Json::Value isp;
        flp["floor"]=Json::Value(floorParams[i].floor);
        flp["mass"]=Json::Value(floorParams[i].mass);
        isp["story"]=Json::Value(interstoryParams[i].story);
        isp["K0"]=Json::Value(interstoryParams[i].K0);
        isp["Sy"]=Json::Value(interstoryParams[i].Sy);
        isp["eta"]=Json::Value(interstoryParams[i].eta);
        isp["C"]=Json::Value(interstoryParams[i].C);
        isp["gamma"]=Json::Value(interstoryParams[i].gamma);
        isp["eta_soft"]=Json::Value(interstoryParams[i].eta_soft);
        isp["alpha"]=Json::Value(interstoryParams[i].alpha);
        isp["beta"]=Json::Value(interstoryParams[i].beta);
        isp["a_k"]=Json::Value(interstoryParams[i].a_k);
        isp["omega"]=Json::Value(interstoryParams[i].omega);
        modelData["interstoryParams"].append(isp);
        modelData["floorParams"].append(flp);
    }

    Json::Value out;
    out["type"]=Json::Value(type);
    out["modelData"]=Json::Value(modelData);

    return out;
}
