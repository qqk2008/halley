#include "aseprite_reader.h"
#include "halley/tools/file/filesystem.h"
#include "halley/core/graphics/sprite/sprite_sheet.h"
#include "halley/file/path.h"
#include "halley/file_formats/image.h"
#include "../assets/importers/sprite_importer.h"
#include "aseprite_file.h"
using namespace Halley;


std::map<String, std::vector<ImageData>> AsepriteReader::importAseprite(String spriteName, gsl::span<const gsl::byte> fileData, bool trim, int padding, bool groupSeparated)
{
	const String baseName = Path(spriteName).getFilename().string();

	AsepriteFile aseFile;
	aseFile.load(fileData);

	const size_t nFrames = aseFile.getNumberOfFrames();
	std::vector<char> frameTagged(nFrames, 0);

	// Load all tags
	std::vector<std::pair<String, std::vector<int>>> tags;
	for (auto& tag: aseFile.getTags()) {
		std::vector<int> frames;
		for (int i = tag.fromFrame; i <= tag.toFrame; ++i) {
			frameTagged[i] = 1;
			frames.push_back(i);
		}
		tags.emplace_back(tag.name, std::move(frames));
	}

	// Create empty tag
	{
		std::vector<int> untagged;
		for (int i = 0; i < int(frameTagged.size()); ++i) {
			if (frameTagged[i] == 0) {
				untagged.push_back(i);
			}
		}
		if (!untagged.empty()) {
			tags.emplace_back("", std::move(untagged));
		}
	}

	// Create frames
	std::map<String, std::vector<ImageData>> frameData;	
	for (auto& t: tags) {
		int i = 0;
		String sequence = t.first;
		String direction = "";
		if (sequence.find(':') != std::string::npos) {
			auto split = sequence.split(':');
			sequence = split.at(0);
			direction = split.at(1);
		}

		for (const int frameN: t.second) {

			std::map<String, std::unique_ptr<Image>> groupFrameImages = aseFile.makeGroupFrameImages(frameN, groupSeparated);

			const auto duration = aseFile.getFrame(frameN).duration;
			const auto hasFrameNumber = t.second.size() > 1;

			for (auto& groupFrameImage : groupFrameImages)
			{
				std::vector<ImageData> groupFrameData;
				auto firstImage = frameData.find(groupFrameImage.first) == frameData.end();
				addImageData(i, groupFrameData, std::move(groupFrameImage.second), aseFile, baseName, sequence, direction, duration, trim, padding, hasFrameNumber, groupFrameImage.first, firstImage, spriteName);
				
				if(frameData.find(groupFrameImage.first) == frameData.end())
				{
					frameData[groupFrameImage.first] = std::vector<ImageData>();
				}
				std::move(groupFrameData.begin(), groupFrameData.end(), std::back_inserter(frameData[groupFrameImage.first]));
			}
			++i;
		}
	}

	return frameData;
}

void AsepriteReader::addImageData(int frameNumber, std::vector<ImageData>& frameData, std::unique_ptr<Image> frameImage, const AsepriteFile& aseFile,
	const String& baseName, const String& sequence, const String& direction, int duration, bool trim, int padding, bool hasFrameNumber, std::optional<String> group,
	bool firstImage, const String& spriteName)
{
	frameData.emplace_back();
	auto& imgData = frameData.back();

	Rect4i clip = trim ? frameImage->getTrimRect() : frameImage->getRect();
	if (!clip.isEmpty()) { // Padding an empty sprite can have all kinds of unexpected effects, and also affect performance
		clip = clip.grow(padding);
	}
	
	imgData.img = std::move(frameImage);
	imgData.frameNumber = frameNumber;
	imgData.sequenceName = sequence;
	imgData.direction = direction;
	imgData.duration = duration;
	imgData.clip = clip;
	
	std::stringstream ss;
	ss << baseName.cppStr();
	if (imgData.sequenceName != "") {
		ss << "_" << imgData.sequenceName.cppStr();
	}
	if (imgData.direction != "") {
		ss << "_" << imgData.direction.cppStr();
	}
	
	if (!group->isEmpty())
	{
		ss << "_" << group.value();
	}
	if (hasFrameNumber) {
		ss << "_" << std::setw(3) << std::setfill('0') << imgData.frameNumber;
	}
	
	imgData.filenames.emplace_back(ss.str());
	if (firstImage) {
		firstImage = false;
		imgData.filenames.emplace_back(":img:" + spriteName + (!group->isEmpty() ? ":" + group.value() : ""));
	}
}
