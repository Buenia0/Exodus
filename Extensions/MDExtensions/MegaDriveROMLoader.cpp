#include "MegaDriveROMLoader.h"
#include "FileOpenMenuHandler.h"
#include "HierarchicalStorage/HierarchicalStorage.pkg"
#include "DataConversion/DataConversion.pkg"
#include "WindowsSupport/WindowsSupport.pkg"

//----------------------------------------------------------------------------------------------------------------------
// Constructors
//----------------------------------------------------------------------------------------------------------------------
MegaDriveROMLoader::MegaDriveROMLoader(const std::wstring& implementationName, const std::wstring& instanceName, unsigned int moduleID)
:Extension(implementationName, instanceName, moduleID), _menuHandler(0)
{ }

//----------------------------------------------------------------------------------------------------------------------
MegaDriveROMLoader::~MegaDriveROMLoader()
{
	// Delete the menu handler
	delete _menuHandler;
}

//----------------------------------------------------------------------------------------------------------------------
// Window functions
//----------------------------------------------------------------------------------------------------------------------
bool MegaDriveROMLoader::RegisterSystemMenuHandler()
{
	// Create the menu handler
	_menuHandler = new FileOpenMenuHandler(*this);
	_menuHandler->LoadMenuItems();
	return true;
}

//----------------------------------------------------------------------------------------------------------------------
void MegaDriveROMLoader::AddSystemMenuItems(SystemMenu systemMenu, IMenuSegment& menuSegment)
{
	if (systemMenu == SystemMenu::File)
	{
		IMenuSegment& isolatedMenuSegment = menuSegment.AddMenuItemSegment();
		_menuHandler->AddMenuItems(isolatedMenuSegment);
	}
}

//----------------------------------------------------------------------------------------------------------------------
// ROM loading functions
//----------------------------------------------------------------------------------------------------------------------
void MegaDriveROMLoader::LoadROMFile()
{
	//##TODO## Add a path for autogenerated modules to our global preferences?
	std::wstring autoGeneratedROMModuleFolderPath = PathCombinePaths(GetGUIInterface().GetGlobalPreferencePathModules(), L"AutoGenerated");

	// Use the saved general ROM directory preference as the initial folder location when searching for the target ROM
	// file. If no directory is saved, we let Windows choose the initial search location.
	std::wstring initialSearchFolderPath;
	HierarchicalStorageNode generalROMDirectoryNode;
	if (GetGUIInterface().GetGlobalPreference(L"Paths.GeneralROMDirectory", generalROMDirectoryNode))
	{
		initialSearchFolderPath = generalROMDirectoryNode.ExtractData<std::wstring>();
	}

	// Select a target file
	std::wstring selectedFilePath;
	if (!GetGUIInterface().SelectExistingFile(L"Mega Drive ROM file|bin|gen|md", L"gen", L"", initialSearchFolderPath, true, selectedFilePath))
	{
		return;
	}

	// Update the general ROM directory preference to use the folder of the selected file
	std::wstring selectedFileDirectory = PathGetDirectory(selectedFilePath);
	generalROMDirectoryNode.SetData(selectedFileDirectory);
	GetGUIInterface().SetGlobalPreference(L"Paths.GeneralROMDirectory", generalROMDirectoryNode);

	// Build a module definition for the target ROM file
	std::wstring romName;
	HierarchicalStorageTree tree;
	if (!BuildROMFileModuleFromFile(selectedFilePath, tree.GetRootNode(), romName))
	{
		return;
	}

	// Generate the file path for the output XML file
	std::wstring autoGeneratedModuleOutputPath = PathCombinePaths(autoGeneratedROMModuleFolderPath, GetExtensionInstanceName());
	std::wstring moduleFileName = romName + L".xml";
	std::wstring moduleFilePath = PathCombinePaths(autoGeneratedModuleOutputPath, moduleFileName);

	// Write the generated module structure to the target output file
	if (!SaveOutputROMModule(tree, moduleFilePath))
	{
		return;
	}

	// Retrieve the current running state of the system, and stop the system if it is
	// currently running.
	ISystemExtensionInterface& system = GetSystemInterface();
	bool systemRunningState = system.SystemRunning();
	system.StopSystem();

	// If we currently have at least one ROM module loaded, and the system reports that we
	// can't currently load the new ROM module, assume we're currently using up all
	// available connectors, and unload the oldest of the currently loaded ROM modules.
	IGUIExtensionInterface& gui = GetGUIInterface();
	if (!_currentlyLoadedROMModuleFilePaths.empty() && !gui.CanModuleBeLoaded(moduleFilePath))
	{
		// Unload the oldest currently loaded ROM module
		std::wstring oldestLoadedROMModule = *_currentlyLoadedROMModuleFilePaths.begin();
		UnloadROMFileFromModulePath(oldestLoadedROMModule);
		_currentlyLoadedROMModuleFilePaths.pop_front();
	}

	// Trigger an initialization of the system now that we're about to load a new ROM
	// module
	system.FlagInitialize();

	// Attempt to load the module file we just generated back into the system
	if (!gui.LoadModuleFromFile(moduleFilePath))
	{
		std::wstring text = L"Failed to load the generated module definition file. Check the event log for further info.";
		std::wstring title = L"Error loading ROM!";
		SafeMessageBox((HWND)GetGUIInterface().GetMainWindowHandle(), text, title, MB_ICONEXCLAMATION);
		return;
	}

	// Restore the running state of the system
	if (systemRunningState)
	{
		system.RunSystem();
	}

	// Record information on this loaded module
	_currentlyLoadedROMModuleFilePaths.push_back(moduleFilePath);
}

//----------------------------------------------------------------------------------------------------------------------
void MegaDriveROMLoader::UnloadROMFile()
{
	// If at least one ROM module is currently loaded, unload the oldest module.
	if (!_currentlyLoadedROMModuleFilePaths.empty())
	{
		// Stop the system if it is currently running
		GetSystemInterface().StopSystem();

		// Unload the oldest currently loaded ROM module
		std::wstring oldestLoadedROMModule = *_currentlyLoadedROMModuleFilePaths.begin();
		UnloadROMFileFromModulePath(oldestLoadedROMModule);
		_currentlyLoadedROMModuleFilePaths.pop_front();
	}
}

//----------------------------------------------------------------------------------------------------------------------
void MegaDriveROMLoader::UnloadROMFileFromModulePath(const std::wstring& targetROMModulePath) const
{
	// Retrieve the set of ID numbers for all currently loaded modules
	std::list<unsigned int> loadedModuleIDs = GetSystemInterface().GetLoadedModuleIDs();

	// Attempt to retrieve the ID of the first matching loaded module file
	bool foundLoadedModuleID = false;
	unsigned int loadedModuleID = { };
	std::list<unsigned int>::const_iterator loadedModuleIDIterator = loadedModuleIDs.begin();
	while (!foundLoadedModuleID && (loadedModuleIDIterator != loadedModuleIDs.end()))
	{
		LoadedModuleInfo moduleInfo;
		if (GetSystemInterface().GetLoadedModuleInfo(*loadedModuleIDIterator, moduleInfo))
		{
			if (moduleInfo.GetModuleFilePath() == targetROMModulePath)
			{
				foundLoadedModuleID = true;
				loadedModuleID = moduleInfo.GetModuleID();
			}
		}
		++loadedModuleIDIterator;
	}

	// If we managed to locate a module which was loaded from the target module file,
	// unload it.
	if (foundLoadedModuleID)
	{
		GetGUIInterface().UnloadModule(loadedModuleID);
	}
}

//----------------------------------------------------------------------------------------------------------------------
// ROM module generation
//----------------------------------------------------------------------------------------------------------------------
bool MegaDriveROMLoader::BuildROMFileModuleFromFile(const std::wstring& filePath, IHierarchicalStorageNode& node, std::wstring& romName)
{
	//##TODO## Add control over these features. They should be added as system settings
	// for this extension.
	bool autoSelectSystemRegion = true;
	bool autoDetectBackupRAMSupport = true;

	// Extract the name of the target file from the file path
	IGUIExtensionInterface& guiInterface = GetGUIInterface();
	std::vector<std::wstring> pathElements = guiInterface.PathSplitElements(filePath);
	std::wstring fileName = PathGetFileName(*pathElements.rbegin());

	// Load the ROM header from the target file
	MegaDriveROMHeader romHeader;
	if (!LoadROMHeaderFromFile(filePath, romHeader))
	{
		std::wstring text = L"Could not read ROM header data from file.";
		std::wstring title = L"Error loading ROM!";
		SafeMessageBox((HWND)guiInterface.GetMainWindowHandle(), text, title, MB_ICONEXCLAMATION);
		return false;
	}

	// Set the name of this loaded ROM
	romName = fileName;

	// If the user has requested us to detect backup RAM support, attempt to do so now.
	bool sramPresent = false;
	unsigned int sramByteSize = { };
	unsigned int sramStartLocation = { };
	bool sramMapOnEvenBytes = { };
	bool sramMapOnOddBytes = { };
	bool sram16Bit = { };
	std::vector<unsigned char> initialRAMData;
	if (autoDetectBackupRAMSupport)
	{
		sramPresent = AutoDetectBackupRAMSupport(romHeader, sramStartLocation, sramByteSize, sramMapOnEvenBytes, sramMapOnOddBytes, sram16Bit, initialRAMData);
	}

	// If the reported SRAM region is seen to overlap with the ROM data, disable SRAM
	// support, since we don't currently support bankswitching.
	if (sramStartLocation < romHeader.fileSize)
	{
		sramPresent = false;
	}

	// Set all required attributes on the root node for this module definition
	node.SetName(L"Module");
	node.CreateAttribute(L"SystemClassName", L"SegaMegaDrive");
	node.CreateAttribute(L"ModuleClassName", fileName);
	node.CreateAttribute(L"ModuleInstanceName", fileName);
	node.CreateAttribute(L"ProgramModule", true);

	// Determine the size of the ROM region to allocate. We use the ROM file size rather
	// than the recorded ROM region in the file header, since this information is
	// essentially unused, and may be incorrect, especially for homebrew ROM files.
	unsigned int romRegionSize = romHeader.fileSize;

	// Calculate the size of the ROM region to allocated for the specified ROM file. We pad
	// out the size of the specified ROM file to the nearest power of two in order to do
	// this. Bytes above the region defined within the specified ROM file will be filled
	// with zeros.
	//##TODO## Provide a way to specify the data to be padded out with 0xFFFF.
	Data romRegionSizePadded(sizeof(romHeader.fileSize) * Data::BitsPerByte, romRegionSize);
	unsigned int highestSetBitMask;
	romRegionSizePadded.GetHighestSetBitMask(highestSetBitMask);
	if (romRegionSize > highestSetBitMask)
	{
		romRegionSizePadded = (highestSetBitMask << 1);
	}

	// Calculate the address mask for the calculated ROM region size
	unsigned int highestSetBitNumber;
	romRegionSizePadded.GetHighestSetBitNumber(highestSetBitNumber);
	unsigned int romRegionAddressMask = (1 << (highestSetBitNumber + 1)) - 1;

	// Add all required child elements for this module definition
	node.CreateChild(L"System.ImportConnector").CreateAttribute(L"ConnectorClassName", L"CartridgePort").CreateAttribute(L"ConnectorInstanceName", L"Cartridge Port");
	node.CreateChild(L"System.ImportBusInterface").CreateAttribute(L"ConnectorInstanceName", L"Cartridge Port").CreateAttribute(L"BusInterfaceName", L"BusInterface").CreateAttribute(L"ImportName", L"BusInterface");
	node.CreateChild(L"System.ImportSystemLine").CreateAttribute(L"ConnectorInstanceName", L"Cartridge Port").CreateAttribute(L"SystemLineName", L"CART").CreateAttribute(L"ImportName", L"CART");
	node.CreateChild(L"Device").CreateAttribute(L"DeviceName", L"ROM16").CreateAttribute(L"InstanceName", L"ROM").CreateAttribute(L"BinaryDataPresent", true).CreateAttribute(L"SeparateBinaryData", true).SetData(filePath);
	if (sramPresent)
	{
		IHierarchicalStorageNode& ramDeviceNode = node.CreateChild(L"Device").CreateAttribute(L"DeviceName", L"RAM8").CreateAttribute(L"InstanceName", L"SRAM").CreateAttributeHex(L"MemoryEntryCount", sramByteSize, 0).CreateAttribute(L"RepeatData", true).CreateAttribute(L"PersistentData", true);
		if (!initialRAMData.empty())
		{
			ramDeviceNode.InsertBinaryData(initialRAMData, L"RAM.InitialData", true);
		}
	}
	node.CreateChild(L"BusInterface.MapDevice").CreateAttribute(L"BusInterfaceName", L"BusInterface").CreateAttribute(L"DeviceInstanceName", L"ROM").CreateAttribute(L"CELineConditions", L"FCCPUSpace=0, CE0=1").CreateAttributeHex(L"MemoryMapBase", 0, 6).CreateAttributeHex(L"MemoryMapSize", romRegionSizePadded.GetData(), 0).CreateAttributeHex(L"AddressMask", romRegionAddressMask, 0).CreateAttribute(L"AddressDiscardLowerBitCount", 1);
	if (sramPresent)
	{
		//##TODO## Generate address masks and shift counts here rather than using address
		// line mappings

		// Calculate the size of the SRAM region in the address space. If the SRAM is
		// 16-bit, the region size is simply the byte size of the SRAM. If the SRAM is
		// 8-bit, the region size is twice the byte size.
		unsigned int sramRegionSize = (sram16Bit)? sramByteSize: (sramByteSize * 2);

		// Determine any write CE line conditions that apply
		std::wstring extraCEWriteConditions;
		if (!sram16Bit && (sramMapOnEvenBytes != sramMapOnOddBytes))
		{
			extraCEWriteConditions = L", LWR=";
			extraCEWriteConditions += (sramMapOnOddBytes)? L"1": L"0";
		}

		node.CreateChild(L"BusInterface.MapDevice").CreateAttribute(L"BusInterfaceName", L"BusInterface").CreateAttribute(L"DeviceInstanceName", L"SRAM").CreateAttribute(L"CELineConditions", L"FCCPUSpace=0, CE0=1, R/W=1").CreateAttributeHex(L"MemoryMapBase", sramStartLocation, 6).CreateAttributeHex(L"MemoryMapSize", sramRegionSize, 0).CreateAttribute(L"AddressLineMapping", L"[09][08][07][06][05][04][03][02][01]").CreateAttribute(L"DataLineMapping", L"[07][06][05][04][03][02][01][00]");
		node.CreateChild(L"BusInterface.MapDevice").CreateAttribute(L"BusInterfaceName", L"BusInterface").CreateAttribute(L"DeviceInstanceName", L"SRAM").CreateAttribute(L"CELineConditions", L"FCCPUSpace=0, CE0=1, R/W=0" + extraCEWriteConditions).CreateAttributeHex(L"MemoryMapBase", sramStartLocation, 6).CreateAttributeHex(L"MemoryMapSize", sramRegionSize, 0).CreateAttribute(L"AddressLineMapping", L"[09][08][07][06][05][04][03][02][01]").CreateAttribute(L"DataLineMapping", L"[07][06][05][04][03][02][01][00]");
	}
	node.CreateChild(L"System.SetLineState").CreateAttribute(L"SystemLineName", L"CART").CreateAttribute(L"Value", 1);

	// If automatic selection of the preferred compatible system region is enabled for
	// games that require specific region codes, attempt to detect the region now, and
	// output region selection elements to the module definition.
	if (autoSelectSystemRegion)
	{
		std::wstring regionCode;
		if (AutoDetectRegionCode(romHeader, regionCode))
		{
			node.CreateChild(L"System.ImportSystemSetting").CreateAttribute(L"ConnectorInstanceName", L"Cartridge Port").CreateAttribute(L"SystemSettingName", L"Region").CreateAttribute(L"ImportName", L"Region");
			node.CreateChild(L"System.SelectSettingOption").CreateAttribute(L"SettingName", L"Region").CreateAttribute(L"OptionName", regionCode);
		}
	}

	return true;
}

//----------------------------------------------------------------------------------------------------------------------
bool MegaDriveROMLoader::SaveOutputROMModule(IHierarchicalStorageTree& tree, const std::wstring& filePath)
{
	// Ensure the target output directory exists
	std::wstring fileDir = PathGetDirectory(filePath);
	if (!CreateDirectory(fileDir, true))
	{
		std::wstring text = L"Could not create the target module output directory.";
		std::wstring title = L"Error loading ROM!";
		SafeMessageBox((HWND)GetGUIInterface().GetMainWindowHandle(), text, title, MB_ICONEXCLAMATION);
		return false;
	}

	// Create the output module file
	Stream::File moduleFile(Stream::IStream::TextEncoding::UTF8);
	if (!moduleFile.Open(filePath, Stream::File::OpenMode::WriteOnly, Stream::File::CreateMode::Create))
	{
		std::wstring text = L"Could not create the output module definition file.";
		std::wstring title = L"Error loading ROM!";
		SafeMessageBox((HWND)GetGUIInterface().GetMainWindowHandle(), text, title, MB_ICONEXCLAMATION);
		return false;
	}
	moduleFile.InsertByteOrderMark();

	// Save the generated module XML data to the output module file
	if (!tree.SaveTree(moduleFile))
	{
		std::wstring text = L"Could not save XML structure to output definition file.";
		std::wstring title = L"Error loading ROM!";
		SafeMessageBox((HWND)GetGUIInterface().GetMainWindowHandle(), text, title, MB_ICONEXCLAMATION);
		return false;
	}

	// Close the generated module file now that we are finished writing it
	moduleFile.Close();

	return true;
}

//----------------------------------------------------------------------------------------------------------------------
// ROM analysis functions
//----------------------------------------------------------------------------------------------------------------------
bool MegaDriveROMLoader::LoadROMHeaderFromFile(const std::wstring& filePath, MegaDriveROMHeader& romHeader) const
{
	IGUIExtensionInterface& guiInterface = GetGUIInterface();
	Stream::IStream* dataStream = guiInterface.OpenExistingFileForRead(filePath);
	if (dataStream == 0)
	{
		return false;
	}

	// Validate the size of the selected file
	if (dataStream->Size() < 0x200)
	{
		guiInterface.DeleteFileStream(dataStream);
		return false;
	}

	// Read in the contents of the Mega Drive ROM header from the file
	dataStream->SetStreamPos(0x100);
	bool result = true;
	result &= dataStream->ReadTextFixedLengthBufferAsASCII(0x10, romHeader.segaString);
	result &= dataStream->ReadTextFixedLengthBufferAsASCII(0x10, romHeader.copyrightString);
	result &= dataStream->ReadTextFixedLengthBufferAsASCII(0x30, romHeader.gameTitleJapan);
	result &= dataStream->ReadTextFixedLengthBufferAsASCII(0x30, romHeader.gameTitleOverseas);
	result &= dataStream->ReadTextFixedLengthBufferAsASCII(0x0E, romHeader.versionString);
	result &= dataStream->ReadDataBigEndian(romHeader.checksum);
	result &= dataStream->ReadTextFixedLengthBufferAsASCII(0x10, romHeader.controllerString);
	result &= dataStream->ReadDataBigEndian(romHeader.romLocationStart);
	result &= dataStream->ReadDataBigEndian(romHeader.romLocationEnd);
	result &= dataStream->ReadDataBigEndian(romHeader.ramLocationStart);
	result &= dataStream->ReadDataBigEndian(romHeader.ramLocationEnd);
	result &= dataStream->ReadDataBigEndian(romHeader.bramSetting[0]);
	result &= dataStream->ReadDataBigEndian(romHeader.bramSetting[1]);
	result &= dataStream->ReadDataBigEndian(romHeader.bramSetting[2]);
	result &= dataStream->ReadDataBigEndian(romHeader.bramSetting[3]);
	result &= dataStream->ReadDataBigEndian(romHeader.bramLocationStart);
	result &= dataStream->ReadDataBigEndian(romHeader.bramLocationEnd);
	result &= dataStream->ReadTextFixedLengthBufferAsASCII(0x4, romHeader.bramUnusedData);
	result &= dataStream->ReadTextFixedLengthBufferAsASCII(0x30, romHeader.unknownString);
	result &= dataStream->ReadTextFixedLengthBufferAsASCII(0x10, romHeader.regionString);

	// Record additional information about the ROM file
	romHeader.fileSize = (unsigned int)dataStream->Size();

	// Return the result of the operation
	guiInterface.DeleteFileStream(dataStream);
	return result;
}

//----------------------------------------------------------------------------------------------------------------------
bool MegaDriveROMLoader::AutoDetectRegionCode(const MegaDriveROMHeader& romHeader, std::wstring& regionCode)
{
	bool specificRegionCodeDetected = false;
	std::string regionString = romHeader.regionString;

	// Decode the traditional JUE region encoding characters if present
	bool regionCodePresentJ = (regionString.find('J') != std::string::npos);
	bool regionCodePresentU = (regionString.find('U') != std::string::npos);
	bool regionCodePresentE = (regionString.find('E') != std::string::npos);
	bool regionCodePresentJPAL = false;

	// Decode the newer binary region tag if present
	if (regionString.find_first_of("014589CD") == 0)
	{
		Data regionCodeInt(4, HexCharToNybble(regionString[0]));
		regionCodePresentJ = regionCodeInt.GetBit(0);
		regionCodePresentU = regionCodeInt.GetBit(2);
		regionCodePresentE = regionCodeInt.GetBit(3);
	}

	// Apply specific overrides for known errors in Mega Drive region header information
	if (StringStartsWith(regionString, "EUROPE"))
	{
		// Check for a region code indicator of "EUROPE". This has seen to be used in
		//"Another World (E) [!]". This ROM is only compatible with the European system,
		// and will show the top part of the screen incorrectly if played on a US system.
		// For this case, we must not detect the "U" in "EUROPE" as a US region identifier.
		regionCodePresentJ = false;
		regionCodePresentU = false;
		regionCodePresentE = true;
		regionCodePresentJPAL = false;
	}
	else if (StringStartsWith(romHeader.versionString, "GM  T-15083"))
	{
		// The Japanese version of flashback is marked with a JUE region code, but it's a
		// Japan only build.
		regionCodePresentJ = true;
		regionCodePresentU = false;
		regionCodePresentE = false;
		regionCodePresentJPAL = false;
	}
	else if (StringStartsWith(romHeader.versionString, "GM  T-79066"))
	{
		// The European version of flashback is marked with a JUE region code, but it's a
		// Europe only build. The sound will be out of sync if this is run on an NTSC
		// system.
		regionCodePresentJ = true;
		regionCodePresentU = false;
		regionCodePresentE = false;
		regionCodePresentJPAL = false;
	}
	else if (StringStartsWith(romHeader.versionString, "GM 00004054-00"))
	{
		// The revision "00" build of Quackshot is an NTSC build only, despite being marked
		// with a JUE region code. The sound gets out of sync on a PAL system. We override
		// the region code here, and force a "U" region code. The revision "01" build of
		// Quackshot doesn't have this problem.
		regionCodePresentJ = false;
		regionCodePresentU = true;
		regionCodePresentE = false;
		regionCodePresentJPAL = false;
	}

	// Select the best region based on region preference
	//##TODO## Allow the user to control the region preference
	if (regionCodePresentJ && regionCodePresentU && regionCodePresentE)
	{
		// If this ROM is marked as compatible with all regions, don't change the system
		// region setting.
	}
	else if (regionCodePresentU)
	{
		regionCode = L"U";
		specificRegionCodeDetected = true;
	}
	else if (regionCodePresentE)
	{
		regionCode = L"E";
		specificRegionCodeDetected = true;
	}
	else if (regionCodePresentJ)
	{
		regionCode = L"J";
		specificRegionCodeDetected = true;
	}
	else if (regionCodePresentJPAL)
	{
		regionCode = L"JPAL";
		specificRegionCodeDetected = true;
	}

	return specificRegionCodeDetected;
}

//----------------------------------------------------------------------------------------------------------------------
bool MegaDriveROMLoader::AutoDetectBackupRAMSupport(const MegaDriveROMHeader& romHeader, unsigned int& sramStartLocation, unsigned int& sramByteSize, bool& linkedToEvenAddress, bool& linkedToOddAddress, bool& sram16Bit, std::vector<unsigned char>& initialRAMData)
{
	// Ensure the identifying "RA" marker indicating backup RAM is present
	if (((char)romHeader.bramSetting[0] != 'R') || ((char)romHeader.bramSetting[1] != 'A'))
	{
		return false;
	}

	// Decode the BRAM mode bytes
	//         ---------------------------------
	//         | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
	// 0x1B2    |-------------------------------|
	//         | 1 |Sav| 1 |Address| 0 | 0 | 0 |
	//         ---------------------------------
	//         ---------------------------------
	//         | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
	// 0x1B3    |-------------------------------|
	//         | 0 | 0 | 1 | 0 | 0 | 0 | 0 | 0 |
	//         ---------------------------------
	// Sav:     Memory is backup RAM
	// Address: 10 = Memory on even addresses only
	//         11 = Memory on odd addresses only
	//         00 = Memory on even and odd addresses
	Data bramSetting1B2(Data::BitsPerByte, romHeader.bramSetting[2]);
	bool memoryAddressRestricted = bramSetting1B2.GetBit(4);
	bool memoryAvailableOnOddAddresses = bramSetting1B2.GetBit(3);
	linkedToEvenAddress = !memoryAddressRestricted || !memoryAvailableOnOddAddresses;
	linkedToOddAddress = !memoryAddressRestricted || memoryAvailableOnOddAddresses;

	// Decode the SRAM data from the header
	//##FIX## Do this properly. Right now we're overriding the "linkedToOddAddress" value
	// we calculated above, and we're making a lot of assumptions in the SRAM location and
	// size calculations.
	//##TODO## Run all dumped Mega Drive games through our detection code to spit out a
	// table of what games record what for SRAM data, then compare it with our list of
	// games known to use backup RAM, and cross-check the findings.
	linkedToOddAddress = ((romHeader.bramLocationStart & 0x1) != 0);
	sramStartLocation = (romHeader.bramLocationStart & 0xFFFFFFFE);
	unsigned int sramEndLocation = (romHeader.bramLocationEnd | 0x00000001);
	sramByteSize = ((romHeader.bramLocationEnd - sramStartLocation) / 2) + 1;

	// No known games use 16-bit SRAM, but since in theory it's possible, and we want our
	// code to be as flexible as possible, we provide this variable so that we can easily
	// add support for it later.
	//##TODO## Determine if 16-bit SRAM is used in any commercial games
	sram16Bit = false;

	// According to the "Genesis Software Manual", "Addendum 4", SRAM is usually
	// initialized to 0xFF at the factory. Although they state this cannot be relied upon,
	// we use this as the best factory-state we can reliably supply for the initial
	// contents of SRAM.
	initialRAMData.push_back((unsigned char)0xFF);

	// Overrides for game-specific issues
	if (StringStartsWith(romHeader.versionString, "GM T-26013"))
	{
		//"Psy-O-Blade Moving Adventure" has invalid data in the start and end locations.
		// The correct values appear 2 bytes down from the normal location.
		sramStartLocation = 0x200000;
		sramEndLocation = 0x203FFF;
	}
	else if ((romHeader.versionString.empty() || (romHeader.versionString[0] == '\0')) && (romHeader.checksum == 0x8104))
	{
		//"Xin Qi Gai Wang Zi", aka, Beggar Prince, has a bad header, which is missing all
		// the standard information. In this case, if the version string is empty, and the
		// value recorded in the checksum field matches, we assume this is that game.
		//##FIX## I'm not convinced our text buffers will end up empty with this corrupted
		// header. Our current stream code will read the data into the string regardless.
		// The first character of the version string will be null however, since that's
		// what's written in the header.
		sramStartLocation = 0x400000;
		sramEndLocation = 0x40FFFF;
	}

	return true;
}

//----------------------------------------------------------------------------------------------------------------------
bool MegaDriveROMLoader::StringStartsWith(const std::string& targetString, const std::string& compareString)
{
	return (targetString.compare(0, compareString.length(), compareString) == 0);
}
