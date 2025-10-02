#ifndef EMPTYFOLDERSFILESYSTEMMODEL_H
#define EMPTYFOLDERSFILESYSTEMMODEL_H

#include <QFileSystemModel>

class EmptyFoldersFileSystemModel : public QFileSystemModel
{
	Q_OBJECT

public:
	explicit EmptyFoldersFileSystemModel(QObject *parent = nullptr);

	bool hasChildren(const QModelIndex &parent) const override;
};

#endif // EMPTYFOLDERSFILESYSTEMMODEL_H
