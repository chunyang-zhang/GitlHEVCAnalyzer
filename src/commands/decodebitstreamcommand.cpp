#include "decodebitstreamcommand.h"
#include "model/modellocator.h"
#include "parsers/bitstreamparser.h"
#include "parsers/spsparser.h"
#include "parsers/decodergeneralparser.h"
#include "parsers/cupuparser.h"
#include "parsers/tuparser.h"
#include "parsers/predparser.h"
#include "parsers/mvparser.h"
#include "parsers/mergeparser.h"
#include "parsers/intraparser.h"
#include "exceptions/decodingfailexception.h"
#include "gitlupdateuievt.h"
#include <QDir>


DecodeBitstreamCommand::DecodeBitstreamCommand(QObject *parent) :
    GitlAbstractCommand(parent)
{
}

bool DecodeBitstreamCommand::execute( GitlCommandParameter& rcInputArg, GitlCommandParameter& rcOutputArg )
{
    ModelLocator* pModel = ModelLocator::getInstance();

    /// *****STEP 0 : Request*****
    QVariant vValue;
    vValue = rcInputArg.getParameter("filename");
    QString strFilename = vValue.toString();
    vValue = rcInputArg.getParameter("version");
    int iVersion = vValue.toInt();
    vValue = rcInputArg.getParameter("skip_decode");
    bool bSkipDecode = vValue.toBool();
    QString strDecoderPath = pModel->getPreferences().getDecoderFolder();
    QString strDecoderOutputPath = pModel->getPreferences().getCacheFolder();
    int iSequenceIndex = pModel->getSequenceManager().getAllSequences().size();
    strDecoderOutputPath += QString("/%1").arg(iSequenceIndex);




    /// show busy dialog
    GitlUpdateUIEvt evt;
    evt.setParameter("busy_dialog_visible", true);
    evt.dispatch();

    /// hide busy dialog when scope exit
    SCOPE_EXIT(GitlUpdateUIEvt evt;
               evt.setParameter("busy_dialog_visible", false);
               evt.dispatch();
               );


    //TODO BUG Memory Leaking when exception happens
    ComSequence* pcSequence = new ComSequence();
    pcSequence->init();

    /// *****STEP 1 : Use the special decoder to parse bitstream*****
    /// call decoder process to decode bitstream to YUV and output text info
    GitlUpdateUIEvt cDecodingStageInfo;
    bool bSuccess = false;
    if( !bSkipDecode )
    {
        cDecodingStageInfo.setParameter("decoding_progress", "(1/10)Start Decoding Bitstream...");
        dispatchEvt(cDecodingStageInfo);
        BitstreamParser cBitstreamParser;
        bSuccess = cBitstreamParser.parseFile(strDecoderPath,
                                              iVersion,
                                              strFilename,
                                              strDecoderOutputPath,
                                              pcSequence);
        if( !bSuccess )
            throw DecodingFailException();
        qDebug() << "decoding finished";
    }
    else
    {
        bSuccess = true;
        qDebug() << "decoding skiped";
    }


    /// *****STEP 2 : Parse the txt file generated by decoder*****
    /// Parse decoder_sps.txt
    QString strSPSFilename = strDecoderOutputPath + "/decoder_sps.txt";
    if( bSuccess )
    {
        cDecodingStageInfo.setParameter("decoding_progress", "(2/10)Start Parsing Sequence Parameter Set...");
        dispatchEvt(cDecodingStageInfo);
        QFile cSPSFile(strSPSFilename);
        cSPSFile.open(QIODevice::ReadOnly);
        QTextStream cSPSTextStream(&cSPSFile);
        SpsParser cSpsParser;
        bSuccess = cSpsParser.parseFile( &cSPSTextStream, pcSequence );
        cSPSFile.close();
        qDebug() << "SPS file parsing finished";
    }
    /// Parse decoder_general.txt
    QString strGeneralFilename = strDecoderOutputPath + "/decoder_general.txt";
    if( bSuccess )
    {
        cDecodingStageInfo.setParameter("message", "(3/10)Start Parsing Decoder Std Output File...");
        dispatchEvt(cDecodingStageInfo);
        QFile cGeneralFile(strGeneralFilename);
        cGeneralFile.open(QIODevice::ReadOnly);
        QTextStream cGeneralTextStream(&cGeneralFile);
        DecoderGeneralParser cDecoderGeneralParser;
        bSuccess = cDecoderGeneralParser.parseFile( &cGeneralTextStream, pcSequence );
        cGeneralFile.close();
        qDebug() << "Decoder general file parsing finished";
    }

    /// Parse decoder_cupu.txt
    QString strCUPUFilename = strDecoderOutputPath + "/decoder_cupu.txt";
    if( bSuccess )
    {
        cDecodingStageInfo.setParameter("decoding_progress", "(4/10)Start Parsing CU & PU Structure...");
        dispatchEvt(cDecodingStageInfo);
        QFile cCUPUFile(strCUPUFilename);
        cCUPUFile.open(QIODevice::ReadOnly);
        QTextStream cCUPUTextStream(&cCUPUFile);
        CUPUParser cCUPUParser;
        bSuccess = cCUPUParser.parseFile( &cCUPUTextStream, pcSequence );
        cCUPUFile.close();
        qDebug() << "CU&PU file parsing finished";
    }
    /// TODO Parse deocder_tu.txt
    QString strTUFilename = strDecoderOutputPath + "/decoder_tu.txt";
    if( bSuccess )
    {
        cDecodingStageInfo.setParameter("decoding_progress", "(5/10)Start Parsing TU Structure...");
        dispatchEvt(cDecodingStageInfo);
        QFile cTUFile(strTUFilename);
        cTUFile.open(QIODevice::ReadOnly);
        QTextStream cTUTextStream(&cTUFile);
        TUParser cTUParser;
        bSuccess = cTUParser.parseFile( &cTUTextStream, pcSequence );
        cTUFile.close();
        qDebug() << "TU file parsing finished";
    }

    /// Parse decoder_pred.txt
    QString strPredFilename = strDecoderOutputPath + "/decoder_pred.txt";
    if( bSuccess )
    {
        cDecodingStageInfo.setParameter("decoding_progress", "(6/10)Start Parsing Predction Mode...");
        dispatchEvt(cDecodingStageInfo);
        QFile cPredFile(strPredFilename);
        cPredFile.open(QIODevice::ReadOnly);
        QTextStream cPredTextStream(&cPredFile);
        PredParser cPredParser;
        bSuccess = cPredParser.parseFile( &cPredTextStream, pcSequence );
        cPredFile.close();
        qDebug() << "Prediction file parsing finished";
    }

    /// Parse decoder_mv.txt
    QString strMVFilename = strDecoderOutputPath + "/decoder_mv.txt";
    if( bSuccess )
    {
        cDecodingStageInfo.setParameter("decoding_progress", "(7/10)Start Parsing Motion Vectors...");
        dispatchEvt(cDecodingStageInfo);
        QFile cMVFile(strMVFilename);
        cMVFile.open(QIODevice::ReadOnly);
        QTextStream cMVTextStream(&cMVFile);
        MVParser cMVParser;
        bSuccess = cMVParser.parseFile( &cMVTextStream, pcSequence );
        cMVFile.close();
        qDebug() << "MV file parsing finished";
    }

    /// Parse decoder_merge.txt
    QString strMergeFilename = strDecoderOutputPath + "/decoder_merge.txt";
    if( bSuccess )
    {
        cDecodingStageInfo.setParameter("decoding_progress", "(8/10)Start Parsing Motion Merge Info...");
        dispatchEvt(cDecodingStageInfo);
        QFile cMergeFile(strMergeFilename);
        cMergeFile.open(QIODevice::ReadOnly);
        QTextStream cMergeTextStream(&cMergeFile);
        MergeParser cMergeParser;
        bSuccess = cMergeParser.parseFile( &cMergeTextStream, pcSequence );
        cMergeFile.close();
        qDebug() << "Merge file parsing finished";
    }

    /// Parse decoder_intra.txt
    QString strIntraFilename = strDecoderOutputPath + "/decoder_intra.txt";
    if( bSuccess )
    {
        cDecodingStageInfo.setParameter("decoding_progress", "(9/10)Start Parsing Intra Info...");
        dispatchEvt(cDecodingStageInfo);
        QFile cIntraFile(strIntraFilename);
        cIntraFile.open(QIODevice::ReadOnly);
        QTextStream cIntraTextStream(&cIntraFile);
        IntraParser cIntraParser;
        bSuccess = cIntraParser.parseFile( &cIntraTextStream, pcSequence );
        cIntraFile.close();
        qDebug() << "Intra file parsing finished";
    }

    ///*****STEP 3 : Open decoded YUV sequence*****
    pModel->getSequenceManager().addSequence(pcSequence);
    pModel->getSequenceManager().setCurrentSequence(pcSequence);

    QString strYUVFilename = strDecoderOutputPath + "/decoder_yuv.yuv";
    int iWidth  = pModel->getSequenceManager().getCurrentSequence().getWidth();
    int iHeight = pModel->getSequenceManager().getCurrentSequence().getHeight();
    QPixmap* pcFramePixmap = NULL;
    if( bSuccess )
    {
        cDecodingStageInfo.setParameter("decoding_progress", "(10/10)Reding & Drawing Reconstructed YUV...");
        dispatchEvt(cDecodingStageInfo);
        pModel->getFrameBuffer().openYUVFile(strYUVFilename, iWidth, iHeight);
        pcFramePixmap = pModel->getFrameBuffer().getFrame(0);   ///< Read Frame Buffer
        pcFramePixmap = pModel->getDrawEngine().drawFrame(pcSequence, 0, pcFramePixmap);  ///< Draw Frame Buffer

    }


    ///*****STEP 4 : Respond*****
    int iCurrentPoc = pModel->getFrameBuffer().getPoc();
    int iTotalFrame = pModel->getSequenceManager().getCurrentSequence().getTotalFrames();
    rcOutputArg.setParameter("picture",    QVariant::fromValue((void*)(pcFramePixmap)) );
    rcOutputArg.setParameter("current_frame_poc", iCurrentPoc);
    rcOutputArg.setParameter("total_frame_num", iTotalFrame);




    /// nofity sequence list to update
    QVector<ComSequence*>* ppcSequences = &(pModel->getSequenceManager().getAllSequences());
    rcOutputArg.setParameter("sequences",QVariant::fromValue((void*)ppcSequences));
    rcOutputArg.setParameter("current_sequence",QVariant::fromValue((void*)pcSequence));

    return bSuccess;
}