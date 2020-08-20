#include "test_runner.h"
#include <functional>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <list>

using namespace std;


struct Email {
  string from;
  string to;
  string body;
};


class Worker {
public:
  virtual ~Worker() = default;
  virtual void Process(unique_ptr<Email> email) = 0;
  virtual void Run() {
    // только первому worker-у в пайплайне нужно это имплементировать
    throw logic_error("Unimplemented");
  }

protected:
  // реализации должны вызывать PassOn, чтобы передать объект дальше
  // по цепочке обработчиков
  unique_ptr<Worker> next;

  void PassOn(unique_ptr<Email> email) const {
	  if (next.get())
		  next->Process(move(email));
  }

public:
  void SetNext(unique_ptr<Worker> next) {
	  this->next = move(next);
  }
};


class Reader : public Worker {
public:
	Reader(istream& input_) : input(input_) {

	}
	virtual void Process(unique_ptr<Email> email) {
		throw logic_error("Unimplemented");
	}

	virtual void Run() override {
		while (input) {
			auto email = make_unique<Email>();
			if (getline(input, email->from) && getline(input, email->to) && getline(input, email->body))
				PassOn(move(email));
		}
	}

private:
	 istream& input;
};


class Filter : public Worker {
public:
  using Function = function<bool(const Email&)>;

private:
  Function func;

public:
  Filter(Function func_) : func(func_) {

  }

  virtual void Process(unique_ptr<Email> email) override {
	  if (func(*email))
		  PassOn(move(email));
  }
};


class Copier : public Worker {
public:
	Copier(const string& to_) : to(to_) {

	}

	virtual void Process(unique_ptr<Email> email) override {
		if (email->to == to) {
			PassOn(move(email));
		} else {
			auto new_email = make_unique<Email>(*email);
			new_email->to = to;

			PassOn(move(email));
			PassOn(move(new_email));
		}
	}

private:
	string to;
};


class Sender : public Worker {
public:
	Sender(ostream& output_) : output(output_) {

	}

	virtual void Process(unique_ptr<Email> email) override {

		output << email->from << endl;
		output << email->to << endl;
		output << email->body << endl;

		PassOn(move(email));
	}

private:
	ostream& output;
};


// реализуйте класс
class PipelineBuilder {
public:
  // добавляет в качестве первого обработчика Reader
  explicit PipelineBuilder(istream& in) {
	  workers.push_back(make_unique<Reader>(in));
  }

  // добавляет новый обработчик Filter
  PipelineBuilder& FilterBy(Filter::Function filter) {
	  workers.push_back(make_unique<Filter>(filter));
	  return *this;
  }

  // добавляет новый обработчик Copier
  PipelineBuilder& CopyTo(string recipient) {
	  workers.push_back(make_unique<Copier>(recipient));
	  return *this;
  }

  // добавляет новый обработчик Sender
  PipelineBuilder& Send(ostream& out) {
	  workers.push_back(make_unique<Sender>(out));
	  return *this;
  }

  // возвращает готовую цепочку обработчиков
  unique_ptr<Worker> Build() {
	  for (auto it = next(workers.rbegin()); it != workers.rend(); ++it) {
		  (*it)->SetNext(move(*prev(it)));
	  }
	  return move(workers.front());
  }
private:
  list<unique_ptr<Worker>> workers;
};


void TestSanity() {
  string input = (
    "erich@example.com\n"
    "richard@example.com\n"
    "Hello there\n"

    "erich@example.com\n"
    "ralph@example.com\n"
    "Are you sure you pressed the right button?\n"

    "ralph@example.com\n"
    "erich@example.com\n"
    "I do not make mistakes of that kind\n"
  );
  istringstream inStream(input);
  ostringstream outStream;

  PipelineBuilder builder(inStream);
  builder.FilterBy([](const Email& email) {
    return email.from == "erich@example.com";
  });
  builder.CopyTo("richard@example.com");
  builder.Send(outStream);
  auto pipeline = builder.Build();

  pipeline->Run();

  string expectedOutput = (
    "erich@example.com\n"
    "richard@example.com\n"
    "Hello there\n"

    "erich@example.com\n"
    "ralph@example.com\n"
    "Are you sure you pressed the right button?\n"

    "erich@example.com\n"
    "richard@example.com\n"
    "Are you sure you pressed the right button?\n"
  );

  ASSERT_EQUAL(expectedOutput, outStream.str());
}

int main() {
  TestRunner tr;
  RUN_TEST(tr, TestSanity);
  return 0;
}
