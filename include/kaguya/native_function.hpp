#pragma once

#include <string>
#include <vector>

#include "kaguya/config.hpp"
#include "kaguya/utility.hpp"
#include "kaguya/type.hpp"
#include "kaguya/lua_ref.hpp"


namespace kaguya
{
	typedef const std::vector<LuaRef>& VariadicArgType;

	namespace nativefunction
	{
		struct BaseInvoker
		{
			virtual bool checktype(lua_State *state, bool strictcheck) = 0;
			virtual int invoke(lua_State *state) = 0;
			virtual std::string argumentTypeNames() = 0;
			virtual ~BaseInvoker() {}
		};
		
		struct FunctorType :standard::shared_ptr<BaseInvoker>
		{
			typedef standard::shared_ptr<BaseInvoker> base_ptr_;
			FunctorType() {}
			FunctorType(const standard::shared_ptr<BaseInvoker>& ptr) :base_ptr_(ptr) {}

			template<typename T>FunctorType(T f) : standard::shared_ptr<BaseInvoker>(create(f))
			{
			}

#include "kaguya/gen/constructor.inl"

			template<typename CLASS>static FunctorType VariadicConstructorInvoker()
			{
				return FunctorType(base_ptr_(new VariadicArgConstructorInvoker<CLASS>()));
			}
		private:
#include "kaguya/gen/native_function.inl"

			template<class ClassType, class MemType>
			struct MemDataInvoker :BaseInvoker {
				typedef MemType ClassType::*data_type;
				data_type data_;
				MemDataInvoker(data_type data) :data_(data) {}
				virtual bool checktype(lua_State *state, bool strictcheck) {
					if (lua_gettop(state) != 1 && lua_gettop(state) != 2) { return false; }
					if (getPtr(state) == 0) { return false; }
					return true;
				}
				ClassType* getPtr(lua_State *state) {
					if (types::checkType(state, 1, types::typetag<ClassType*>()))
					{
						return types::get(state, 1, types::typetag<ClassType*>());
					}

					if (types::checkType(state, 1, types::typetag<standard::shared_ptr<ClassType>*>()))
					{
						standard::shared_ptr<ClassType>* shared_ptr = types::get(state, 1, types::typetag<standard::shared_ptr<ClassType>*>());
						if (shared_ptr) {
							return shared_ptr->get();
						}
					}
					return 0;
				}
				virtual int invoke(lua_State *state)
				{
					ClassType* ptr = getPtr(state);
					if (lua_gettop(state) == 2)
					{
						ptr->*data_ = types::get(state, 2, types::typetag<MemType>());
					}

					return types::push(state, standard::forward<MemType>(ptr->*data_));
				}

				virtual std::string argumentTypeNames() {
					std::string result;
					result += typeid(ClassType).name();
					result += std::string("[opt]") + typeid(MemType).name();
					return result;
				}
			};
			template< class ClassType, class MemType>
			base_ptr_ create(MemType ClassType::*data)
			{
				typedef MemDataInvoker<ClassType, MemType> invoker_type;
				return standard::shared_ptr<BaseInvoker>(new invoker_type(data));
			}

#if KAGUYA_USE_DECLTYPE
			template <typename T>
			struct lambda_fun : public lambda_fun<decltype(&T::operator())> {};

			template <typename T, typename Ret, typename... Args>
			struct lambda_fun<Ret(T::*)(Args...) const> {
				typedef standard::function<Ret(Args...)> type;
			};

			//for lambda and bind
			template< typename L>
			base_ptr_ create(L l)
			{
				return create(typename lambda_fun<L>::type(l));
			}
#endif

			//variadic argment
			template<typename Ret>
			struct VariadicArgInvoker :BaseInvoker {
				typedef Ret(*func_type)(VariadicArgType);
				func_type func_;
				VariadicArgInvoker(func_type fun) :func_(fun) {}
				virtual bool checktype(lua_State *state, bool) { return true; }
				virtual int invoke(lua_State *state)
				{
					std::vector<LuaRef> args;
					int top = lua_gettop(state);
					args.reserve(top + 1);
					for (int i = 1; i <= top; ++i)
					{
						args.push_back(types::get(state, i, types::typetag<LuaRef>()));
					}
					Ret r = func_(args);
					return types::push(state, standard::forward<Ret>(r));
				}
				virtual std::string argumentTypeNames() {
					return "VariadicArg";
				}
			};
			template<typename Ret>
			base_ptr_ create(Ret(*fun)(VariadicArgType))
			{
				typedef VariadicArgInvoker<Ret> caller_type;
				return base_ptr_(new caller_type(fun));
			}

			struct VariadicArgVoidInvoker :BaseInvoker {
				typedef void(*func_type)(VariadicArgType);
				func_type func_;
				VariadicArgVoidInvoker(func_type fun) :func_(fun) {}
				virtual bool checktype(lua_State *state, bool) { return true; }
				virtual int invoke(lua_State *state)
				{
					std::vector<LuaRef> args;
					int top = lua_gettop(state);
					args.reserve(top + 1);
					for (int i = 1; i <= top; ++i)
					{
						args.push_back(types::get(state, i, types::typetag<LuaRef>()));
					}
					func_(args);
					return 0;
				}

				virtual std::string argumentTypeNames() {
					return "VariadicArg";
				}
			};
			inline base_ptr_ create(void(*fun)(VariadicArgType))
			{
				typedef VariadicArgVoidInvoker caller_type;
				return base_ptr_(new caller_type(fun));
			}

			template <typename Ret, typename T>
			struct VariadicArgMemFunInvoker :BaseInvoker {
				typedef Ret(T::*func_type)(VariadicArgType);
				func_type func_;
				VariadicArgMemFunInvoker(func_type fun) :func_(fun) {}
				virtual bool checktype(lua_State *state, bool) { return true; }
				virtual int invoke(lua_State *state)
				{
					T* t = types::get(state, 1, types::typetag<T*>());
					if (!t) { return 0; }

					std::vector<LuaRef> args;
					int top = lua_gettop(state);
					args.reserve(top);
					for (int i = 2; i <= top; ++i)
					{
						args.push_back(types::get(state, i, types::typetag<LuaRef>()));
					}

					Ret r = (t->*func_)(args);
					return types::push(state, standard::forward<Ret>(r));
				}


				virtual std::string argumentTypeNames() {
					std::string result;
					result += typeid(T).name();
					result += ",VariadicArg";
					return result;
				}
			};
			template <typename Ret, typename T>
			base_ptr_ create(Ret(T::*fun)(VariadicArgType))
			{
				typedef VariadicArgMemFunInvoker<Ret, T> caller_type;
				return base_ptr_(new caller_type(fun));
			}

			template <typename T>
			struct VariadicArgMemVoidFunInvoker :BaseInvoker {
				typedef void (T::*func_type)(VariadicArgType);
				func_type func_;
				VariadicArgMemVoidFunInvoker(func_type fun) :func_(fun) {}
				virtual bool checktype(lua_State *state, bool) { return true; }
				virtual int invoke(lua_State *state)
				{
					T* t = types::get(state, 1, types::typetag<T*>());
					if (!t) { return 0; }

					std::vector<LuaRef> args;
					int top = lua_gettop(state);
					args.reserve(top);
					for (int i = 2; i <= top; ++i)
					{
						args.push_back(types::get(state, i, types::typetag<LuaRef>()));
					}

					(t->*func_)(args);
					return 0;
				}
				virtual std::string argumentTypeNames() {
					std::string result;
					result += typeid(T).name();
					result += ",VariadicArg";
					return result;
				}
			};
			template <typename T>
			base_ptr_ create(void (T::*fun)(VariadicArgType))
			{
				typedef VariadicArgMemVoidFunInvoker<T> caller_type;
				return base_ptr_(new caller_type(fun));
			}


			template <typename Ret, typename T>
			struct VariadicArgConstMemFunInvoker :BaseInvoker {
				typedef Ret(T::*func_type)(VariadicArgType)const;
				func_type func_;
				VariadicArgConstMemFunInvoker(func_type fun) :func_(fun) {}
				virtual bool checktype(lua_State *state, bool) { return true; }
				virtual int invoke(lua_State *state)
				{
					T* t = types::get(state, 1, types::typetag<T*>());
					if (!t) { return 0; }

					std::vector<LuaRef> args;
					int top = lua_gettop(state);
					args.reserve(top - 1);
					for (int i = 2; i <= top; ++i)
					{
						args.push_back(types::get(state, i, types::typetag<LuaRef>()));
					}

					Ret r = (t->*func_)(args);
					return types::push(state, standard::forward <Ret>(r));
				}
				virtual std::string argumentTypeNames() {
					std::string result;
					result += typeid(T).name();
					result += ",VariadicArg";
					return result;
				}
			};
			template <typename Ret, typename T>
			base_ptr_ create(Ret(T::*fun)(VariadicArgType)const)
			{
				typedef VariadicArgConstMemFunInvoker<Ret, T> caller_type;
				return base_ptr_(new caller_type(fun));
			}

			template <typename T>
			struct VariadicArgConstMemVoidFunInvoker :BaseInvoker {
				typedef void (T::*func_type)(VariadicArgType)const;
				func_type func_;
				VariadicArgConstMemVoidFunInvoker(func_type fun) :func_(fun) {}
				virtual bool checktype(lua_State *state, bool) { return true; }
				virtual int invoke(lua_State *state)
				{
					T* t = types::get(state, 1, types::typetag<T*>());
					if (!t) { return 0; }
					std::vector<LuaRef> args;
					int top = lua_gettop(state);
					args.reserve(top - 1);
					for (int i = 2; i <= top; ++i)
					{
						args.push_back(types::get(state, i, types::typetag<LuaRef>()));
					}

					(t->*func_)(args);
					return 0;
				}
				virtual std::string argumentTypeNames() {
					std::string result;
					result += typeid(T).name();
					result += ",VariadicArg";
					return result;
				}
			};
			template <typename T>
			base_ptr_ create(void (T::*fun)(VariadicArgType)const)
			{
				typedef VariadicArgConstMemVoidFunInvoker<T> caller_type;
				return base_ptr_(new caller_type(fun));
			}


			template<typename Ret>
			struct VariadicArgFunInvoker :BaseInvoker {
				typedef standard::function<Ret(VariadicArgType)> func_type;
				func_type func_;
				VariadicArgFunInvoker(func_type fun) :func_(fun) {}
				virtual bool checktype(lua_State *state, bool) { return true; }
				virtual int invoke(lua_State *state)
				{
					std::vector<LuaRef> args;
					int top = lua_gettop(state);
					args.reserve(top + 1);
					for (int i = 1; i <= top; ++i)
					{
						args.push_back(types::get(state, i, types::typetag<LuaRef>()));
					}
					Ret r = func_(args);
					return types::push(state, standard::forward<Ret>(r));
				}
				virtual std::string argumentTypeNames() {
					return "VariadicArg";
				}
			};
			template<typename Ret>
			base_ptr_ create(standard::function<Ret(VariadicArgType)> fun)
			{
				typedef VariadicArgFunInvoker<Ret> caller_type;
				return base_ptr_(new caller_type(fun));
			}

			struct VariadicArgVoidFunInvoker :BaseInvoker {
				typedef standard::function<void(VariadicArgType)> func_type;
				func_type func_;
				VariadicArgVoidFunInvoker(func_type fun) :func_(fun) {}
				virtual bool checktype(lua_State *state, bool) { return true; }
				virtual int invoke(lua_State *state)
				{
					std::vector<LuaRef> args;
					int top = lua_gettop(state);
					args.reserve(top + 1);
					for (int i = 1; i <= top; ++i)
					{
						args.push_back(types::get(state, i, types::typetag<LuaRef>()));
					}
					func_(args);
					return 0;
				}
				virtual std::string argumentTypeNames() {
					return "VariadicArg";
				}
			};
			inline base_ptr_ create(standard::function<void(VariadicArgType)> fun)
			{
				typedef VariadicArgVoidFunInvoker caller_type;
				return base_ptr_(new caller_type(fun));
			}

			template<typename CLASS>
			struct VariadicArgConstructorInvoker :BaseInvoker {
				VariadicArgConstructorInvoker() {}
				virtual bool checktype(lua_State *state, bool) { return true; }

				virtual int invoke(lua_State *state)
				{
					std::vector<LuaRef> args;
					int top = lua_gettop(state);
					args.reserve(top + 1);
					for (int i = 1; i <= top; ++i)
					{
						args.push_back(types::get(state, i, types::typetag<LuaRef>()));
					}
					void *storage = lua_newuserdata(state, sizeof(CLASS));
					new(storage) CLASS(args);
					types::class_userdata::setmetatable<CLASS>(state);
					return 1;
				}
				virtual std::string argumentTypeNames() {
					return "VariadicArg";
				}
			};
		};

		inline FunctorType* pick_match_function(lua_State *l)
		{
			int overloadnum = int(lua_tonumber(l, lua_upvalueindex(1)));

			if (overloadnum == 1)
			{
				FunctorType* fun = static_cast<FunctorType*>(lua_touserdata(l, lua_upvalueindex(2)));

				if (!(*fun)->checktype(l, false))
				{
					return 0;
				}
				return fun;
			}
			FunctorType* weak_match = 0;
			for (int i = 0; i < overloadnum; ++i)
			{
				FunctorType* fun = static_cast<FunctorType*>(lua_touserdata(l, lua_upvalueindex(i + 2)));
				if (!fun || !(*fun))
				{
					continue;
				}
				if ((*fun)->checktype(l, true))
				{
					return fun;
				}
				else if (weak_match == 0 && (*fun)->checktype(l, false))
				{
					weak_match = fun;
				}
			}
			return weak_match;
		}
		inline std::string build_arg_error_message(lua_State *l)
		{
			std::string message = "argument not matching" + util::argmentTypes(l) + "\t candidated\n";

			int overloadnum = int(lua_tonumber(l, lua_upvalueindex(1)));
			for (int i = 0; i < overloadnum; ++i)
			{
				FunctorType* fun = static_cast<FunctorType*>(lua_touserdata(l, lua_upvalueindex(i + 2)));
				if (!fun || !(*fun))
				{
					continue;
				}
				message += std::string("\t\t") + (*fun)->argumentTypeNames() + "\n";
			}
			return message;
		}
		inline int functor_dispatcher(lua_State *l)
		{
			FunctorType* fun = pick_match_function(l);
			if (fun && (*fun))
			{
				try {
					return (*fun)->invoke(l);
				}
				catch (std::exception & e) {
					util::traceBack(l, e.what());
				}
				catch (...) {
					util::traceBack(l, "Unknown exception");
				}
			}
			else
			{
				util::traceBack(l, build_arg_error_message(l).c_str());
			}
			return lua_error(l);
		}

		inline int functor_destructor(lua_State *state)
		{
			FunctorType* f = types::class_userdata::test_userdata<FunctorType>(state, 1);
			if (f)
			{
				f->~FunctorType();
			}
			return 0;
		}
		inline void reg_functor_destructor(lua_State* state)
		{
			if (types::class_userdata::newmetatable<FunctorType>(state))
			{
				lua_pushcclosure(state, &functor_destructor, 0);
				lua_setfield(state, -2, "__gc");
				lua_setfield(state, -1, "__index");
			}
		}
	}


	typedef nativefunction::FunctorType FunctorType;
	template<typename T>
	inline FunctorType lua_function(T f)
	{
		return FunctorType(standard::forward<T>(f));
	}

	template<typename T>
	inline FunctorType function(T f)
	{
		return FunctorType(standard::forward<T>(f));
	}
	namespace types
	{
		template<>
		inline FunctorType get(lua_State* l, int index, typetag<FunctorType> tag)
		{
			FunctorType* ptr = types::class_userdata::test_userdata<FunctorType>(l, index);
			if (ptr)
			{
				return *ptr;
			}
			return FunctorType();
		}
		template<>
		inline int push(lua_State* l, const FunctorType& f)
		{
			lua_pushnumber(l, 1);//no overload
			void *storage = lua_newuserdata(l, sizeof(FunctorType));
			new(storage) FunctorType(f);
			types::class_userdata::setmetatable<FunctorType>(l);
			lua_pushcclosure(l, &nativefunction::functor_dispatcher, 2);
			return 1;
		}
		template<typename F>
		inline int push_native_function(lua_State* l, const F& f)
		{
			return push(l, FunctorType(f));
		}
#if KAGUYA_USE_RVALUE_REFERENCE
		inline int push(lua_State* l, FunctorType&& f)
		{
			lua_pushnumber(l, 1);//no overload
			void *storage = lua_newuserdata(l, sizeof(FunctorType));
			new(storage) FunctorType(standard::forward<FunctorType>(f));
			types::class_userdata::setmetatable<FunctorType>(l);
			lua_pushcclosure(l, &nativefunction::functor_dispatcher, 2);
			return 1;
		}
#endif
	}
};
