#include "emptyfoldersfilesystemmodel.h"
#include <QDir>

EmptyFoldersFileSystemModel::EmptyFoldersFileSystemModel(QObject *parent)
	: QFileSystemModel(parent)
{
}

bool EmptyFoldersFileSystemModel::hasChildren(const QModelIndex &parent) const
{
	if (!parent.isValid())
		return true; // ������ ������ ����� �����

	QFileInfo fileInfo = this->fileInfo(parent);
	if (!fileInfo.isDir())
		return false;

	// ������� �������� ����� QDir
	QDir dir(fileInfo.absoluteFilePath());
	dir.setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
	return !dir.entryList().isEmpty();

//	// ���������, ���� �� � ����� ��������
//	QDir dir(fileInfo.absoluteFilePath());
//	QFileInfoList entries = dir.entryInfoList(
//		QDir::Dirs | QDir::NoDotAndDotDot,
//		QDir::Name
//	);
//	return !entries.isEmpty();
}
