// 静态单例实例
VectorDataHandler &VectorDataHandler::instance()
{
    static VectorDataHandler instance;
    return instance;
}


VectorDataHandler::VectorDataHandler() : m_cancelRequested(0), m_cacheInitialized(false)
{
    // 构造函数不变
}

VectorDataHandler::~VectorDataHandler()
{
    qDebug() << "VectorDataHandler::~VectorDataHandler - 析构";
    clearCache();
}


void VectorDataHandler::cancelOperation()
{
    m_cancelRequested.storeRelease(1);
}
