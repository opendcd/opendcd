

class IWorker {
 public:
  virtual ~IWorker() = 0;
  virtual void Work() = 0;
};

IWorker::~IWorker() { }

template<unsigned int n>
class Forker : public IWorker {
 public:
  virtual ~Forker() { }
  virtual void Work() {
  }
};

int main() {
  Forker<4> f();
  f.Work();
  return 0;
}
