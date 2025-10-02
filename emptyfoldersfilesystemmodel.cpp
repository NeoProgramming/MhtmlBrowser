#include "emptyfoldersfilesystemmodel.h"
#include <QDir>

EmptyFoldersFileSystemModel::EmptyFoldersFileSystemModel(QObject *parent)
	: QFileSystemModel(parent)
{
}

bool EmptyFoldersFileSystemModel::hasChildren(const QModelIndex &parent) const
{
	if (!parent.isValid())
		return true; // Корень всегда имеет детей

	QFileInfo fileInfo = this->fileInfo(parent);
	if (!fileInfo.isDir())
		return false;

	// Быстрая проверка через QDir
	QDir dir(fileInfo.absoluteFilePath());
	dir.setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
	return !dir.entryList().isEmpty();

//	// Проверяем, есть ли в папке подпапки
//	QDir dir(fileInfo.absoluteFilePath());
//	QFileInfoList entries = dir.entryInfoList(
//		QDir::Dirs | QDir::NoDotAndDotDot,
//		QDir::Name
//	);
//	return !entries.isEmpty();
}
