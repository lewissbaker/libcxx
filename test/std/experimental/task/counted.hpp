// -*- C++ -*-
//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef TEST_EXPERIMENTAL_TASK_COUNTED_HPP
#define TEST_EXPERIMENTAL_TASK_COUNTED_HPP

class counted
{
public:

  counted() : id_(nextId_++)
  {
    ++defaultConstructedCount_;
  }

  counted(const counted& other) : id_(other.id_)
  {
    ++copyConstructedCount_;
  }

  counted(counted&& other) : id_(std::exchange(other.id_, 0))
  {
    ++moveConstructedCount_;
  }

  ~counted()
  {
    ++destructedCount_;
  }

  static void reset()
  {
    nextId_ = 1;
    defaultConstructedCount_ = 0;
    copyConstructedCount_ = 0;
    moveConstructedCount_ = 0;
    destructedCount_ = 0;
  }

  static std::size_t active_instance_count()
  {
    return
      defaultConstructedCount_ +
      copyConstructedCount_ +
      moveConstructedCount_ -
      destructedCount_;
  }

  static std::size_t copy_constructor_count()
  {
    return copyConstructedCount_;
  }

  static std::size_t move_constructor_count()
  {
    return moveConstructedCount_;
  }

  static std::size_t default_constructor_count()
  {
    return defaultConstructedCount_;
  }

  static std::size_t destructor_count()
  {
    return destructedCount_;
  }

  std::size_t id() const { return id_; }

private:
  std::size_t id_;

  inline static std::size_t nextId_;
  inline static std::size_t defaultConstructedCount_;
  inline static std::size_t copyConstructedCount_;
  inline static std::size_t moveConstructedCount_;
  inline static std::size_t destructedCount_;

};

#endif
