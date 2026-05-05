import unreal

wgt_class = unreal.load_class(None, "/Script/SellNoEvil.SNEGameRootWidget")
gm_class  = unreal.load_class(None, "/Script/SellNoEvil.SNEPrototypeGameMode")

wbp = unreal.load_asset("/Game/UI/WBP_GameRoot.WBP_GameRoot")
bp  = unreal.load_asset("/Game/Blueprints/BP_SNEGameMode.BP_SNEGameMode")

print(f"WBP loaded: {wbp}")
print(f"BP  loaded: {bp}")
print(f"WGT class:  {wgt_class}")
print(f"GM  class:  {gm_class}")

unreal.BlueprintEditorLibrary.reparent_blueprint(wbp, wgt_class)
unreal.BlueprintEditorLibrary.compile_blueprint(wbp)
unreal.EditorAssetLibrary.save_loaded_asset(wbp)
print("WBP_GameRoot: reparented, compiled, saved")

unreal.BlueprintEditorLibrary.reparent_blueprint(bp, gm_class)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
unreal.EditorAssetLibrary.save_loaded_asset(bp)
print("BP_SNEGameMode: reparented, compiled, saved")
