#include "tracks/UTrackPoint.hpp"
#include "util/uiutil.hpp"

#include <imgui.h>
#include <magic_enum/magic_enum.hpp>

#include <iostream>
#include <array>
#include <cctype>

namespace {
	std::string TrimToken(const std::string& str) {
		size_t start = 0;
		while (start < str.length() && (std::isspace(str[start]) || str[start] == '\r' || str[start] == '\n')) {
			start++;
		}
		size_t end = str.length();
		while (end > start && (std::isspace(str[end - 1]) || str[end - 1] == '\r' || str[end - 1] == '\n')) {
			end--;
		}
		return str.substr(start, end - start);
	}

    std::string GetSanitizedStartToken(std::stringstream& stream) {
        std::string token;

        while (std::getline(stream, token, ' ')) {
            token = TrimToken(token);

            if (token.empty()) {
                continue;
            }

            // Some files may include "endoftheline" markers that can be merged with
            // the next token by newline separators (e.g. "endoftheline\nc").
            if (token.rfind("endoftheline", 0) == 0) {
                size_t newlinePos = token.find_last_of("\r\n");
                if (newlinePos != std::string::npos && newlinePos + 1 < token.size()) {
                    std::string trailing = TrimToken(token.substr(newlinePos + 1));
                    if (!trailing.empty() && trailing != "endoftheline") {
                        return trailing;
                    }
                }
                continue;
            }

            size_t newlinePos = token.find_last_of("\r\n");
            if (newlinePos != std::string::npos && newlinePos + 1 < token.size()) {
                std::string trailing = TrimToken(token.substr(newlinePos + 1));
                if (!trailing.empty()) {
                    token = trailing;
                }
            }

            return token;
        }

        return "";
    }
}

UTracks::UTrackPoint::UTrackPoint() : mPosition(glm::zero<glm::vec3>()), mHandleA(glm::zero<glm::vec3>()),
    mHandleB(glm::zero<glm::vec3>()), mSomeScalar(0.0f), mStationType(ENodeStationType::None), bIsTunnel(false), bIsJunction(false),
    bIsCurve(false), bHighlighted(false), bSelected(false)
{

}

UTracks::UTrackPoint::UTrackPoint(std::string parentTrackName) : UTrackPoint() {
    mParentTrackName = parentTrackName;
}

UTracks::UTrackPoint::UTrackPoint(const UTracks::UTrackPoint& other) : UTrackPoint() {
    mPosition = other.mPosition;
    mHandleA = other.mHandleA;
    mHandleB = other.mHandleB;
    mParentTrackName = other.mParentTrackName;
    bIsCurve = other.bIsCurve;
    bIsTunnel = other.bIsTunnel;
}

UTracks::UTrackPoint::~UTrackPoint() {

}

void UTracks::UTrackPoint::LoadPoint(std::stringstream& stream) {
    std::string token = "";

    // Read and sanitize first token (should be 'c' for curve or first coordinate)
    token = GetSanitizedStartToken(stream);
    
    if (token.empty()) {
        std::cerr << "Warning: Empty token at start of LoadPoint\n";
        return;
    }

    if (token[0] == 'c') {
        bIsCurve = true;

        // Read curve position X
        std::getline(stream, token, ' ');
        token = TrimToken(token);
        if (!token.empty()) { try { mPosition.x = std::stof(token); } catch (const std::exception& e) { std::cerr << "Failed to parse mPosition.x: '" << token << "' (" << e.what() << ")\n"; mPosition.x = 0.0f; } }

        // Read curve position Y
        std::getline(stream, token, ' ');
        token = TrimToken(token);
        if (!token.empty()) { try { mPosition.y = std::stof(token); } catch (const std::exception& e) { std::cerr << "Failed to parse mPosition.y: '" << token << "' (" << e.what() << ")\n"; mPosition.y = 0.0f; } }

        // Read curve position Z
        std::getline(stream, token, ' ');
        token = TrimToken(token);
        if (!token.empty()) { try { mPosition.z = std::stof(token); } catch (const std::exception& e) { std::cerr << "Failed to parse mPosition.z: '" << token << "' (" << e.what() << ")\n"; mPosition.z = 0.0f; } }

        // Read handle A X
        std::getline(stream, token, ' ');
        token = TrimToken(token);
        if (!token.empty()) { try { mHandleA.x = std::stof(token); } catch (const std::exception& e) { std::cerr << "Failed to parse mHandleA.x: '" << token << "' (" << e.what() << ")\n"; mHandleA.x = 0.0f; } }

        // Read handle A Y
        std::getline(stream, token, ' ');
        token = TrimToken(token);
        if (!token.empty()) { try { mHandleA.y = std::stof(token); } catch (const std::exception& e) { std::cerr << "Failed to parse mHandleA.y: '" << token << "' (" << e.what() << ")\n"; mHandleA.y = 0.0f; } }

        // Read handle A Z
        std::getline(stream, token, ' ');
        token = TrimToken(token);
        if (!token.empty()) { try { mHandleA.z = std::stof(token); } catch (const std::exception& e) { std::cerr << "Failed to parse mHandleA.z: '" << token << "' (" << e.what() << ")\n"; mHandleA.z = 0.0f; } }

        // Read handle B X
        std::getline(stream, token, ' ');
        token = TrimToken(token);
        if (!token.empty()) { try { mHandleB.x = std::stof(token); } catch (const std::exception& e) { std::cerr << "Failed to parse mHandleB.x: '" << token << "' (" << e.what() << ")\n"; mHandleB.x = 0.0f; } }

        // Read handle B Y
        std::getline(stream, token, ' ');
        token = TrimToken(token);
        if (!token.empty()) { try { mHandleB.y = std::stof(token); } catch (const std::exception& e) { std::cerr << "Failed to parse mHandleB.y: '" << token << "' (" << e.what() << ")\n"; mHandleB.y = 0.0f; } }

        // Read handle B Z
        std::getline(stream, token, ' ');
        token = TrimToken(token);
        if (!token.empty()) { try { mHandleB.z = std::stof(token); } catch (const std::exception& e) { std::cerr << "Failed to parse mHandleB.z: '" << token << "' (" << e.what() << ")\n"; mHandleB.z = 0.0f; } }

        // Convert Y/Z coordinates
        float yTmp = mPosition.y;
        mPosition.y = mPosition.z;
        mPosition.z = -yTmp;

        yTmp = mHandleA.y;
        mHandleA.y = mHandleA.z;
        mHandleA.z = -yTmp;

        yTmp = mHandleB.y;
        mHandleB.y = mHandleB.z;
        mHandleB.z = -yTmp;
    }
    else {
        // Non-curve: first token is position X
        try { mPosition.x = std::stof(token); } catch (const std::exception& e) { std::cerr << "Failed to parse mPosition.x: '" << token << "' (" << e.what() << ")\n"; mPosition.x = 0.0f; }
        
        // Read position Y
        std::getline(stream, token, ' ');
        token = TrimToken(token);
        if (!token.empty()) { try { mPosition.y = std::stof(token); } catch (const std::exception& e) { std::cerr << "Failed to parse mPosition.y: '" << token << "' (" << e.what() << ")\n"; mPosition.y = 0.0f; } }
        
        // Read position Z
        std::getline(stream, token, ' ');
        token = TrimToken(token);
        if (!token.empty()) { try { mPosition.z = std::stof(token); } catch (const std::exception& e) { std::cerr << "Failed to parse mPosition.z: '" << token << "' (" << e.what() << ")\n"; mPosition.z = 0.0f; } }

        // Convert Y/Z coordinates
        float yTmp = mPosition.y;
        mPosition.y = mPosition.z;
        mPosition.z = -yTmp;

        mHandleA = mPosition;
        mHandleB = mPosition;
    }

    // Read scalar value
    std::getline(stream, token, ' ');
    token = TrimToken(token);
    if (!token.empty()) { try { mSomeScalar = std::stof(token); } catch (const std::exception& e) { std::cerr << "Failed to parse mSomeScalar: '" << token << "' (" << e.what() << ")\n"; mSomeScalar = 0.0f; } }

    // Read info bits
    uint8_t infoBits = stream.get() - 0x30; // Subtract the value of the char '0' to get the actual value.
    mStationType =  ENodeStationType(infoBits & ENodeInfoBits::BITS_STATION_TYPE);
    bIsTunnel    = (infoBits & ENodeInfoBits::BITS_IS_TUNNEL)   >> 2;
    bIsJunction  = (infoBits & ENodeInfoBits::BITS_IS_JUNCTION) >> 3;

    stream.get();

    if (bIsJunction || mStationType != ENodeStationType::None) {
        std::getline(stream, token, '\n');
        mArgument = std::string(token);
    }
}

void UTracks::UTrackPoint::SavePoint(std::stringstream& stream) {
    if (bIsCurve) {
        stream << "c ";
        stream << mPosition.x << " " << -mPosition.z << " " << mPosition.y << " ";
        stream << mHandleA.x  << " " << -mHandleA.z  << " " << mHandleA.y  << " ";
        stream << mHandleB.x  << " " << -mHandleB.z  << " " << mHandleB.y  << " ";
    }
    else {
        stream << mPosition.x << " " << -mPosition.z << " " << mPosition.y << " ";
    }

    stream << mSomeScalar << " ";

    uint8_t infoBits = 0;
    infoBits |= mStationType & ENodeInfoBits::BITS_STATION_TYPE;
    infoBits |= bIsTunnel   << 2;
    infoBits |= bIsJunction << 3;

    stream << char(infoBits + 0x30);

    if (bIsJunction || mStationType != ENodeStationType::None) {
        stream << " " << mArgument << "\n";
    }
    else {
        stream << "\n";
    }
}

void UTracks::UTrackPoint::TrySetJunctionArgument() {
    if (!bIsJunction || mJunctionPartner.expired()) {
        mArgument = "";
        return;
    }

    mArgument = mJunctionPartner.lock()->GetParentTrackName();
}

void UTracks::UTrackPoint::SetJunctionPartner(std::shared_ptr<UTrackPoint> partner) {
    if (partner == nullptr && !mJunctionPartner.expired()) {
        std::shared_ptr<UTrackPoint> partnerLocked = mJunctionPartner.lock();
        partnerLocked->BreakJunction();

        BreakJunction();
        return;
    }

    if (!bIsJunction) {
        bIsJunction = true;
    }

    mJunctionPartner = partner;
}

void UTracks::UTrackPoint::BreakJunction() {
    bIsJunction = false;
    mJunctionPartner = std::weak_ptr<UTrackPoint>();
}
