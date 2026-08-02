#include "XMLParser.h"


#include <iostream>
#include <memory>
#include <optional>
#include <xercesc/dom/DOM.hpp>
#include <xercesc/dom/DOMDocument.hpp>
#include <xercesc/dom/DOMElement.hpp>
#include <xercesc/dom/DOMNamedNodeMap.hpp>
#include <xercesc/dom/DOMNode.hpp>
#include <xercesc/framework/LocalFileFormatTarget.hpp>
#include <xercesc/framework/LocalFileInputSource.hpp>
#include <xercesc/framework/MemBufFormatTarget.hpp>
#include <xercesc/framework/MemBufInputSource.hpp>
#include <xercesc/parsers/XercesDOMParser.hpp>
#include <xercesc/sax/EntityResolver.hpp>
#include <xercesc/sax/ErrorHandler.hpp>
#include <xercesc/sax/SAXParseException.hpp>
#include <xercesc/util/XMLException.hpp>
#include <xercesc/util/XercesDefs.hpp>
#include <xercesc/util/XercesVersion.hpp>


#include "Console.h"
#include "Exception.h"
#include "FileInfo.h"

#include "FileLock.h"
#include "XMLTools.h"

using namespace XERCES_CPP_NAMESPACE;

namespace
{

// TODO Throw on error
class DOMTreeErrorReporter: public ErrorHandler
{
public:
    // -----------------------------------------------------------------------
    //  Implementation of the error handler interface
    // -----------------------------------------------------------------------
    void warning(const SAXParseException& toCatch) override;
    void error(const SAXParseException& toCatch) override;
    void fatalError(const SAXParseException& toCatch) override;
    void resetErrors() override;

    // -----------------------------------------------------------------------
    //  Getter methods
    // -----------------------------------------------------------------------
    bool getSawErrors() const;

    // -----------------------------------------------------------------------
    //  Private data members
    //
    //  fSawErrors
    //      This is set if we get any errors, and is queryable via a getter
    //      method. Its used by the main code to suppress output if there are
    //      errors.
    // -----------------------------------------------------------------------
    bool fSawErrors {false};
};

inline bool DOMTreeErrorReporter::getSawErrors() const
{
    return fSawErrors;
}

void DOMTreeErrorReporter::warning(const SAXParseException& /*exc*/)
{
    //
    // Ignore all warnings.
    //
}

void DOMTreeErrorReporter::error(const SAXParseException& toCatch)
{
    fSawErrors = true;
    std::cerr << "Error at file \"" << StrX(toCatch.getSystemId()) << "\", line "
              << toCatch.getLineNumber() << ", column " << toCatch.getColumnNumber()
              << "\n   Message: " << StrX(toCatch.getMessage()) << std::endl;
}

void DOMTreeErrorReporter::fatalError(const SAXParseException& toCatch)
{
    fSawErrors = true;
    std::cerr << "Fatal Error at file \"" << StrX(toCatch.getSystemId()) << "\", line "
              << toCatch.getLineNumber() << ", column " << toCatch.getColumnNumber()
              << "\n   Message: " << StrX(toCatch.getMessage()) << std::endl;
}

void DOMTreeErrorReporter::resetErrors()
{
    // No-op in this case
}


class DOMPrintErrorHandler: public DOMErrorHandler
{
public:
    DOMPrintErrorHandler() = default;
    ~DOMPrintErrorHandler() override = default;

    /** @name The error handler interface */
    bool handleError(const DOMError& domError) override;
    void resetErrors()
    {}

    /* Unimplemented constructors and operators */
    DOMPrintErrorHandler(const DOMPrintErrorHandler&) = delete;
    DOMPrintErrorHandler(DOMPrintErrorHandler&&) = delete;
    void operator=(const DOMPrintErrorHandler&) = delete;
    void operator=(DOMPrintErrorHandler&&) = delete;
};

bool DOMPrintErrorHandler::handleError(const DOMError& domError)
{
    // Display whatever error message passed from the serializer
    char* msg = XMLString::transcode(domError.getMessage());
    std::cout << msg << '\n';
    XMLString::release(&msg);

    // Instructs the serializer to continue serialization if possible.
    return true;
}


void InitXercesC()
{
    static bool Init = false;
    if (!Init) {
        try {
            XMLPlatformUtils::Initialize();
        }
        catch (const XMLException& toCatch) {
#if defined(FC_OS_LINUX) || defined(FC_OS_CYGWIN)
            std::ostringstream err;
#else
            std::stringstream err;
#endif
            char* pMsg = XMLString::transcode(toCatch.getMessage());
            err << "Error during Xerces-c Initialization.\n"
                << "  Exception message:" << pMsg;
            XMLString::release(&pMsg);
            throw Base::XMLBaseException(err.str());
        }
        Init = true;
    }
}

class DOMPrintFilter: public DOMLSSerializerFilter
{
public:
    /** @name Constructors */
    explicit DOMPrintFilter(ShowType whatToShow = DOMNodeFilter::SHOW_ALL);
    //@{

    /** @name Destructors */
    ~DOMPrintFilter() override = default;
    //@{

    /** @ interface from DOMWriterFilter */
    FilterAction acceptNode(const DOMNode* node) const override;
    //@{

    ShowType getWhatToShow() const override
    {
        return fWhatToShow;
    }

    // unimplemented copy ctor and assignment operator
    DOMPrintFilter(const DOMPrintFilter&) = delete;
    DOMPrintFilter(DOMPrintFilter&&) = delete;
    DOMPrintFilter& operator=(const DOMPrintFilter&) = delete;
    DOMPrintFilter& operator=(DOMPrintFilter&&) = delete;

    ShowType fWhatToShow;
};

DOMPrintFilter::DOMPrintFilter(ShowType whatToShow)
    : fWhatToShow(whatToShow)
{}

DOMPrintFilter::FilterAction DOMPrintFilter::acceptNode(const DOMNode* node) const
{
    if (XMLString::compareString(node->getNodeName(), XStrLiteral("FCParameters").unicodeForm())
        == 0) {
        // This node is supposed to have a single FCParamGroup and two text nodes.
        // Over time it can happen that the text nodes collect extra newlines.
        const DOMNodeList* children = node->getChildNodes();
        for (XMLSize_t i = 0; i < children->getLength(); i++) {
            DOMNode* child = children->item(i);
            if (child->getNodeType() == DOMNode::TEXT_NODE) {
                child->setNodeValue(XStrLiteral("\n").unicodeForm());
            }
        }
    }

    // clang-format off
    switch (node->getNodeType()) {
        case DOMNode::TEXT_NODE: {
            // Filter out text element if it is under a group node. Note text xml
            // element is plain text in between tags, and we do not store any text
            // there.
            auto parent = node->getParentNode();
            if (parent && XMLString::compareString(parent->getNodeName(),
                                                   XStrLiteral("FCParamGroup").unicodeForm()) == 0) {
                return DOMNodeFilter::FILTER_REJECT;
            }
            return DOMNodeFilter::FILTER_ACCEPT;
        }
        case DOMNode::DOCUMENT_TYPE_NODE:
        case DOMNode::DOCUMENT_NODE: {
            return DOMNodeFilter::FILTER_REJECT;  // no effect
        }
        default: {
            return DOMNodeFilter::FILTER_ACCEPT;
        }
    }
    // clang-format on
}

std::unique_ptr<Base::XMLElement> toXMLElement(XERCES_CPP_NAMESPACE::DOMNode* document)
{
    auto element = std::make_unique<Base::XMLElement>();
    element->tag = StrX(document->getNodeName()).c_str();
    if (document->hasAttributes()) {
        DOMNamedNodeMap* attrs = document->getAttributes();
        for (XMLSize_t i = 0; i < attrs->getLength(); i++) {
            DOMNode* attr = attrs->item(i);
            element->attrs[StrX(attr->getNodeName()).c_str()] = StrX(attr->getNodeValue()).c_str();
        }
    }

    const DOMNodeList* children = document->getChildNodes();
    for (XMLSize_t i = 0; i < children->getLength(); i++) {
        DOMNode* child = children->item(i);
        switch (child->getNodeType()) {
            case XERCES_CPP_NAMESPACE::DOMNode::ELEMENT_NODE:
                element->children.emplace_back(::toXMLElement(child));
                break;
            case XERCES_CPP_NAMESPACE::DOMNode::TEXT_NODE:
                element->content = StrXUTF8(child->getNodeValue()).c_str();
                break;
            default:
                break;
        }
    }
    return element;
}

void toDOMDocument(const Base::XMLElement& source, XERCES_CPP_NAMESPACE::DOMElement* root)
{
    for (const auto& attr : source.attrs) {
        root->setAttribute(
            XStr(attr.first.c_str()).unicodeForm(),
            XStr(attr.second.c_str()).unicodeForm()
        );
    }
    if (!source.content.empty()) {
        root->setTextContent(XStr(source.content.c_str()).unicodeForm());
    }
    for (const auto& child : source.children) {
        auto* childNode = root->getOwnerDocument()->createElement(
            XStr(child->tag.c_str()).unicodeForm()
        );
        root->appendChild(childNode);
        ::toDOMDocument(*child, childNode);
    }
}

void saveDocumentToTarget(DOMDocument* doc, XMLFormatTarget* myFormTarget)
{
    try {
        std::unique_ptr<DOMPrintFilter> myFilter;
        std::unique_ptr<DOMErrorHandler> myErrorHandler;

        // NOLINTBEGIN
        // get a serializer, an instance of DOMWriter
        XMLCh tempStr[100];
        XMLString::transcode("LS", tempStr, 99);
        DOMImplementation* impl = DOMImplementationRegistry::getDOMImplementation(tempStr);
        DOMLSSerializer* theSerializer = static_cast<DOMImplementationLS*>(impl)->createLSSerializer();
        // NOLINTEND

        // set user specified end of line sequence and output encoding
        theSerializer->setNewLine(nullptr);


        if (doc) {
            DOMLSOutput* theOutput = static_cast<DOMImplementationLS*>(impl)->createLSOutput();
            theOutput->setEncoding(nullptr);

            bool gUseFilter = true;
            if (gUseFilter) {
                myFilter = std::make_unique<DOMPrintFilter>(
                    DOMNodeFilter::SHOW_ELEMENT | DOMNodeFilter::SHOW_ATTRIBUTE
                    | DOMNodeFilter::SHOW_DOCUMENT_TYPE | DOMNodeFilter::SHOW_TEXT
                );
                theSerializer->setFilter(myFilter.get());
            }

            // plug in user's own error handler
            myErrorHandler = std::make_unique<DOMPrintErrorHandler>();
            DOMConfiguration* config = theSerializer->getDomConfig();

            // NOLINTBEGIN
            config->setParameter(XMLUni::fgDOMErrorHandler, myErrorHandler.get());

            // set feature if the serializer supports the feature/mode
            if (config->canSetParameter(XMLUni::fgDOMWRTSplitCdataSections, true)) {
                config->setParameter(XMLUni::fgDOMWRTSplitCdataSections, true);
            }

            if (config->canSetParameter(XMLUni::fgDOMWRTDiscardDefaultContent, true)) {
                config->setParameter(XMLUni::fgDOMWRTDiscardDefaultContent, true);
            }

            if (config->canSetParameter(XMLUni::fgDOMWRTFormatPrettyPrint, true)) {
                config->setParameter(XMLUni::fgDOMWRTFormatPrettyPrint, true);
            }
            // NOLINTEND

            theOutput->setByteStream(myFormTarget);
            theSerializer->write(doc, theOutput);

            theOutput->release();
        }

        theSerializer->release();
    }
    catch (XMLException& e) {
        std::cerr << "An error occurred during creation of output transcoder. Msg is:" << std::endl
                  << StrX(e.getMessage()) << std::endl;
    }
}
}  // namespace


namespace Base
{
std::unique_ptr<XMLElement> XMLElement::clone() const
{
    auto result = std::make_unique<XMLElement>();

    result->tag = tag;
    result->attrs = attrs;
    result->content = content;

    result->children.reserve(children.size());

    for (const auto& child : children) {
        result->children.push_back(child->clone());
    }

    return result;
}

std::unique_ptr<XMLElement> parseXMLFile(const fs::path& path)
{
    InitXercesC();

#if defined(FC_OS_WIN32)
    std::wstring name = path.toStdWString();
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    LocalFileInputSource inputSource(reinterpret_cast<const XMLCh*>(name.c_str()));
#else
    LocalFileInputSource inputSource(XStr(path.string().c_str()).unicodeForm());
#endif

    XercesDOMParser parser;
    DOMTreeErrorReporter errReporter {};
    parser.setErrorHandler(&errReporter);

    std::string err;
    try {
        parser.parse(inputSource);
    }
    catch (const XMLException& e) {
        err = std::format("An error occurred during parsing: {}", StrX(e.getMessage()).c_str());
    }
    catch (const DOMException& e) {
        err = std::format("A DOM error occurred during parsing. DOMException code: {}", e.code);
    }
    catch (...) {
        err = "An error occurred during parsing";
    }

    if (!err.empty()) {
        throw XMLBaseException {err};
    }

    XERCES_CPP_NAMESPACE::DOMDocument* document = parser.adoptDocument();

    if (!document || !document->getDocumentElement()) {
        throw XMLBaseException("Malformed document: Invalid document");
    }

    DOMElement* rootElement = document->getDocumentElement();
    auto xmlContent = ::toXMLElement(rootElement);
    document->release();

    return xmlContent;
}

void saveXMLFile(const fs::path& path, const XMLElement& xmlTree)
{
    InitXercesC();

    DOMImplementation* impl = DOMImplementationRegistry::getDOMImplementation(
        XUTF8StrLiteral("Core LS").unicodeForm()
    );
    DOMDocument* doc = impl->createDocument(nullptr, XStr(xmlTree.tag.c_str()).unicodeForm(), nullptr);
    DOMElement* root = doc->getDocumentElement();

    ::toDOMDocument(xmlTree, root);

#if defined(FC_OS_WIN32)
    std::wstring name = file.toStdWString();
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    LocalFileFormatTarget myFormTarget {reinterpret_cast<const XMLCh*>(name.c_str())};
#else
    LocalFileFormatTarget myFormTarget {LocalFileFormatTarget(path.string().c_str())};
#endif

    ::saveDocumentToTarget(doc, &myFormTarget);
}

std::optional<std::string> checkXMLDocument(const XMLElement& xmlTree, const std::string& xsdString)
{
    InitXercesC();

    try {
        DOMImplementation* impl = DOMImplementationRegistry::getDOMImplementation(
            XUTF8StrLiteral("Core LS").unicodeForm()
        );
        DOMDocument* doc
            = impl->createDocument(nullptr, XStr(xmlTree.tag.c_str()).unicodeForm(), nullptr);
        DOMElement* root = doc->getDocumentElement();

        ::toDOMDocument(xmlTree, root);

        //
        // Plug in a format target to receive the resultant
        // XML stream from the serializer.
        //
        // LocalFileFormatTarget prints the resultant XML stream
        // to a file once it receives any thing from the serializer.
        //
        MemBufFormatTarget myFormTarget;
        ::saveDocumentToTarget(doc, &myFormTarget);

        // Either use the file saved on disk or write the current XML into a buffer in memory
        // const char* xmlFile = "...";
        MemBufInputSource xmlFile(myFormTarget.getRawBuffer(), myFormTarget.getLen(), "(memory)");

        // Either load the XSD file from disk or use the built-in string
        // const char* xsdFile = "...";
        std::string xsdStr(xsdString);  // NOLINT
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        MemBufInputSource xsdFile(
            reinterpret_cast<const XMLByte*>(xsdStr.c_str()),
            xsdStr.size(),
            "Parameter.xsd"
        );

        // See
        // http://apache-xml-project.6118.n7.nabble.com/validating-xml-with-xsd-schema-td17515.html
        //
        XercesDOMParser parser;
        Grammar* grammar = parser.loadGrammar(xsdFile, Grammar::SchemaGrammarType, true);
        if (!grammar) {
            throw XMLBaseException {"Grammar file cannot be loaded"};
        }

        parser.setExternalNoNamespaceSchemaLocation("Parameter.xsd");
        // parser.setExitOnFirstFatalError(true);
        // parser.setValidationConstraintFatal(true);
        parser.cacheGrammarFromParse(true);
        parser.setValidationScheme(XercesDOMParser::Val_Auto);
        parser.setDoNamespaces(true);
        parser.setDoSchema(true);
        parser.setDisableDefaultEntityResolution(true);

        DOMTreeErrorReporter errHandler;
        parser.setErrorHandler(&errHandler);
        parser.parse(xmlFile);

        if (parser.getErrorCount() > 0) {
            return std::format("Unexpected XML structure detected: {} errors", parser.getErrorCount());
        }
    }
    catch (XMLException& e) {
        throw XMLBaseException {
            std::format("An error occurred while checking document: {}", StrX(e.getMessage()).c_str())
        };
    }
    return std::nullopt;
}

}  // namespace Base
