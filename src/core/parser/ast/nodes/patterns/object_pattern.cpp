/*******************************************************************************
 * @file src/core/parser/ast/nodes/patterns/object_pattern.cpp
 * @brief ObjectPattern AST ノードクラスの実装ファイル。
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 *
 * @details
 * This file implements the ObjectPattern class methods defined in
 * `object_pattern.h`. It handles validation (property types, RestElement
 * placement), child management, visitor acceptance, and serialization
 * according to ESTree and AeroJS standards.
 * It assumes reuse of the Property node from object expressions, but validates
 * that the property value is a Pattern in this context.
 ******************************************************************************/

#include "src/core/parser/ast/nodes/patterns/object_pattern.h"

#include <cassert>            // For assert()
#include <nlohmann/json.hpp>  // For nlohmann::json implementation
#include <sstream>            // For std::ostringstream in ToString
#include <stdexcept>          // For std::runtime_error in validation

#include "src/core/config/build_config.h"  // For AEROJS_DEBUG
#include "src/core/error/syntax_error.h"
#include "src/core/parser/ast/nodes/expressions/property.h"  // Reusing Property node
#include "src/core/parser/ast/nodes/patterns/rest_spread.h"  // For RestElement
#include "src/core/parser/ast/visitor/ast_visitor.h"         // For ASTVisitor
#include "src/core/util/json_utils.h"                        // For baseJson, safeGetJson
#include "src/core/util/logger.h"                            // For AEROJS_DEBUG_LOG
#include "src/core/util/string_utils.h"                      // For indentString
#include "src/utils/json_writer.h"
#include "src/utils/vector_utils.h"

namespace aerojs::parser::ast {

//=============================================================================
// Constructor & Destructor & Move Semantics
//=============================================================================

ObjectPattern::ObjectPattern(const SourceLocation& location,
                             PropertyList&& properties)
    : Pattern{location, NodeType::OBJECT_PATTERN},
      m_properties{std::move(properties)} {
  AERO_LOG_DEBUG("Creating ObjectPattern at {}", location.ToString());

  // Set parent for children and perform initial structural validation
  for (auto& prop : m_properties) {
    if (prop) {
      prop->SetParent(this);
    } else {
      // Should not happen with valid properties, but handle defensively
      AERO_LOG_WARN("Null property element found during ObjectPattern construction at {}", location.ToString());
      throw error::SyntaxError("Invalid null property in ObjectPattern.", location);
    }
  }

  try {
    InternalValidate();  // Validate RestElement constraints
  } catch (const error::SyntaxError& e) {
    AERO_LOG_ERROR("Validation failed during ObjectPattern construction: {}", e.what());
    // Re-throw or handle as appropriate, possibly wrapping the exception
    throw;
  }
  AERO_LOG_DEBUG("ObjectPattern created successfully with {} properties.", m_properties.size());
}

ObjectPattern::ObjectPattern(ObjectPattern&& other) noexcept
    : Pattern(std::move(other)),  // Move base class members
      m_properties(std::move(other.m_properties)) {
  // Update parent pointers of the moved children
  for (auto& prop : m_properties) {
    if (prop) {
      prop->SetParent(this);
    }
  }
  AERO_LOG_DEBUG("ObjectPattern moved from {}", other.GetSourceLocation().ToString());
}

ObjectPattern& ObjectPattern::operator=(ObjectPattern&& other) noexcept {
  if (this != &other) {
    Pattern::operator=(std::move(other));  // Move assign base class
    m_properties = std::move(other.m_properties);

    // Update parent pointers of the moved children
    for (auto& prop : m_properties) {
      if (prop) {
        prop->SetParent(this);
      }
    }
    AERO_LOG_DEBUG("ObjectPattern move assigned from {}", other.GetSourceLocation().ToString());
  }
  return *this;
}

//=============================================================================
// Public Methods
//=============================================================================

NodeType ObjectPattern::GetType() const noexcept {
  return NodeType::OBJECT_PATTERN;
}

const ObjectPattern::PropertyList& ObjectPattern::GetProperties() const noexcept {
  return m_properties;
}

ObjectPattern::PropertyList& ObjectPattern::GetProperties() noexcept {
  return m_properties;
}

void ObjectPattern::Accept(ASTVisitor& visitor) {
  AERO_LOG_DEBUG("Accepting visitor for ObjectPattern at {}", GetSourceLocation().ToString());
  visitor.VisitObjectPattern(*this);
}

void ObjectPattern::Validate() const {
  AERO_LOG_DEBUG("Validating ObjectPattern at {}", GetSourceLocation().ToString());
  InternalValidate();  // Check RestElement constraints first

  bool hasRest = false;
  for (const auto& prop : m_properties) {
    ASSERT(prop != nullptr, "Property element cannot be null after construction.");

    const NodeType propType = prop->GetType();

    if (propType == NodeType::PROPERTY) {
      // Ensure the value is a Pattern if it exists (Property validation handles this)
      // For destructuring, Property's value *must* be a Pattern or AssignmentPattern
      const auto* propertyNode = static_cast<const Property*>(prop.get());
      if (propertyNode->GetValue()) {  // Check if value exists (e.g., `{ key }` vs `{ key: value }`)
        const NodeType valueType = propertyNode->GetValue()->GetType();
        if (!IsPattern(valueType) && valueType != NodeType::ASSIGNMENT_EXPRESSION /* for defaults */) {
          AERO_LOG_ERROR("Invalid value type ({}) in Property within ObjectPattern at {}. Expected Pattern or AssignmentExpression.",
                         NodeTypeToString(valueType),
                         prop->GetSourceLocation().ToString());
          throw error::SyntaxError("Property value in object pattern must be a pattern or default value.", prop->GetSourceLocation());
        }
      } else if (!propertyNode->IsShorthand()) {
        // If not shorthand, value must exist (parser should ensure this)
        AERO_LOG_ERROR("Non-shorthand Property in ObjectPattern is missing value at {}", prop->GetSourceLocation().ToString());
        throw error::SyntaxError("Invalid property structure in object pattern.", prop->GetSourceLocation());
      }
    } else if (propType == NodeType::REST_ELEMENT) {
      hasRest = true;  // Already validated placement by InternalValidate
      // Ensure the argument is an Identifier (RestElement validation handles this)
      const auto* restNode = static_cast<const RestElement*>(prop.get());
      if (restNode->GetArgument()->GetType() != NodeType::IDENTIFIER) {
        AERO_LOG_ERROR("Invalid argument type ({}) for RestElement in ObjectPattern at {}. Expected Identifier.",
                       NodeTypeToString(restNode->GetArgument()->GetType()),
                       prop->GetSourceLocation().ToString());
        throw error::SyntaxError("Rest element in object pattern must bind to an identifier.", prop->GetSourceLocation());
      }
    } else {
      AERO_LOG_ERROR("Invalid node type ({}) found in ObjectPattern properties at {}. Expected Property or RestElement.",
                     NodeTypeToString(propType),
                     prop->GetSourceLocation().ToString());
      throw error::SyntaxError("Invalid element in object pattern.", prop->GetSourceLocation());
    }

    // Recursively validate the property/rest element itself
    try {
      prop->Validate();
    } catch (const error::SyntaxError& e) {
      AERO_LOG_ERROR("Validation failed for child element ({}) in ObjectPattern: {}", NodeTypeToString(propType), e.what());
      // Re-throw with potentially more context or just re-throw
      throw;
    }
  }
  AERO_LOG_DEBUG("ObjectPattern validation successful.");
}

NodeList ObjectPattern::GetChildren() const {
  NodeList children;
  children.reserve(m_properties.size());
  for (const auto& prop : m_properties) {
    // Ensure we only add valid pointers
    if (prop) {
      children.push_back(prop.get());
    } else {
      AERO_LOG_WARN("Encountered unexpected null property element when getting children for ObjectPattern at {}", GetSourceLocation().ToString());
    }
  }
  return children;
}

std::string ObjectPattern::ToString(const std::string& indent) const {
  std::stringstream ss;
  ss << indent << "ObjectPattern <" << GetSourceLocation().ToString() << ">\n";
  std::string childIndent = indent + "  ";
  if (m_properties.empty()) {
    ss << childIndent << "(Empty)";
  } else {
    ss << childIndent << "Properties:\n";
    std::string propIndent = childIndent + "  ";
    for (const auto& prop : m_properties) {
      if (prop) {
        ss << prop->ToString(propIndent);  // Already includes newline
      } else {
        ss << propIndent << "<Invalid Null Property>\n";
      }
    }
  }
  return ss.str();
}

void ObjectPattern::ToJson(utils::JsonWriter& writer) const {
  writer.StartObject();
  writer.WriteProperty("type", NodeTypeToString(GetType()));
  writer.WriteProperty("loc", GetSourceLocation());

  writer.StartArrayProperty("properties");
  for (const auto& prop : m_properties) {
    if (prop) {
      prop->ToJson(writer);  // Property/RestElement writes its own object
    } else {
      writer.WriteNull();  // Indicate invalid property
      AERO_LOG_WARN("Serializing null property element in ObjectPattern at {}", GetSourceLocation().ToString());
    }
  }
  writer.EndArray();  // properties

  writer.EndObject();
}

std::unique_ptr<Node> ObjectPattern::Clone() const {
  AERO_LOG_DEBUG("Cloning ObjectPattern at {}", GetSourceLocation().ToString());
  PropertyList clonedProperties;
  clonedProperties.reserve(m_properties.size());
  for (const auto& prop : m_properties) {
    if (prop) {
      clonedProperties.push_back(prop->Clone());
    } else {
      // This case should ideally not happen if validation is correct
      clonedProperties.push_back(nullptr);
      AERO_LOG_WARN("Cloning encountered a null property element in ObjectPattern at {}", GetSourceLocation().ToString());
    }
  }
  // Create the new node using the move constructor for the cloned vector
  return std::make_unique<ObjectPattern>(GetSourceLocation(), std::move(clonedProperties));
}

//=============================================================================
// Private Methods
//=============================================================================

void ObjectPattern::InternalValidate() const {
  bool restFound = false;
  for (size_t i = 0; i < m_properties.size(); ++i) {
    const auto& prop = m_properties[i];
    if (!prop) {
      AERO_LOG_ERROR("Null property element found during internal validation of ObjectPattern at {}", GetSourceLocation().ToString());
      throw error::SyntaxError("Invalid null property in object pattern.", GetSourceLocation());
    }

    if (prop->GetType() == NodeType::REST_ELEMENT) {
      if (restFound) {
        AERO_LOG_ERROR("Multiple RestElements found in ObjectPattern at {}", prop->GetSourceLocation().ToString());
        throw error::SyntaxError("Only one rest element is allowed in an object pattern.", prop->GetSourceLocation());
      }
      if (i != m_properties.size() - 1) {
        AERO_LOG_ERROR("RestElement is not the last element in ObjectPattern at {}", prop->GetSourceLocation().ToString());
        throw error::SyntaxError("Rest element must be the last element in an object pattern.", prop->GetSourceLocation());
      }
      restFound = true;
    }
  }
  AERO_LOG_DEBUG("Internal validation of ObjectPattern structure (RestElement) successful.");
}

}  // namespace aerojs::parser::ast