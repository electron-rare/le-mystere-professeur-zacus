#pragma once

namespace zacus_story_portable {
namespace tinyfsm {

struct Event {
  enum class Kind : unsigned char {
    kUnknown = 0,
    kBegin,
    kLoad,
    kUpdate,
    kStop,
  };

  explicit Event(Kind value) : kind(value) {}
  virtual ~Event() = default;
  Kind kind;
};

template <typename Derived>
class State {
 public:
  virtual ~State() = default;
  virtual const char* name() const = 0;
  virtual void onEnter(Derived&) {}
  virtual void onExit(Derived&) {}
  virtual void onEvent(Derived&, const Event&) = 0;
};

template <typename Derived>
class Machine {
 public:
  explicit Machine(Derived& owner, State<Derived>& initial)
      : owner_(owner), current_(&initial) {
    current_->onEnter(owner_);
  }

  void dispatch(const Event& event) {
    current_->onEvent(owner_, event);
  }

  void transition(State<Derived>& next) {
    if (&next == current_) {
      return;
    }
    current_->onExit(owner_);
    current_ = &next;
    current_->onEnter(owner_);
  }

  const State<Derived>& current() const {
    return *current_;
  }

 private:
  Derived& owner_;
  State<Derived>* current_;
};

}  // namespace tinyfsm
}  // namespace zacus_story_portable
