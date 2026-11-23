#include "Papyrus.h"

namespace Papyrus
{
	bool Call(const char* a_function, std::string a_args)
	{
		const auto game = RE::GameVM::GetSingleton();
		const auto vm = game ? game->GetVM() : nullptr;
		if (!vm) {
			return false;
		}

		// No result callback. CommonLibSF only forward-declares IStackCallbackFunctor, so the
		// empty pointer is created once and never destroyed (destroying it needs the full type).
		static const auto* noCallback = new RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor>();

		return vm->DispatchStaticCall(
			RE::BSFixedString("Transporter_SF"),
			RE::BSFixedString(a_function),
			[args = std::move(a_args)](RE::BSScrapArray<RE::BSScript::Variable>& a_out) {
				a_out.resize(1);
				a_out[0] = RE::BSFixedString(args.c_str());
				return true;
			},
			*noCallback,
			0);
	}
}
