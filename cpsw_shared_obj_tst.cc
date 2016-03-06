#include <stdio.h>
#include <string>
#include <cpsw_shared_obj.h>


class CGoodSub;
class CNoClonSub;
typedef shared_ptr<CGoodSub> GoodSub;
typedef shared_ptr<CNoClonSub> NoClonSub;

class TestFailed {
public:
	std::string s_;
	TestFailed(const char *m):s_(m){}
};

class CGoodSub : public CShObj {
protected:
	CGoodSub(const CGoodSub &orig, Key &k):CShObj(orig, k)
	{
		printf("GoodSub copy cons\n");
	}
public:
	CGoodSub(Key &k):CShObj(k){ printf("GoodSub constructor\n"); }
	~CGoodSub() {printf("GoodSub destructor\n"); }
	virtual CGoodSub *clone(Key &k)
	{
		printf("GoodSub clone\n");
		return new CGoodSub( *this, k );
	}
};

class CNoClonSub : public CShObj {
public:
	CNoClonSub(Key &k):CShObj(k){}
};


int
main(int argc, char **argv)
{
GoodSub p1 = CShObj::create<GoodSub>();

GoodSub p1c = CShObj::clone( p1 );

}
