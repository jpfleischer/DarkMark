// DarkMark (C) 2019-2024 Stephane Charette <stephanecharette@gmail.com>

#include "DarkMark.hpp"


dm::ClassIdWnd::ClassIdWnd(File project_dir, const std::string & fn) :
	DocumentWindow("DarkMark - " + File(fn).getFileName(), Colours::darkgrey, TitleBarButtons::closeButton),
	ThreadWithProgressWindow("DarkMark", true, true),
	done					(false),
	dir						(project_dir),
	names_fn				(fn),
	error_count				(0),
	add_button				("Add Row"),
	up_button				("up"	, 0.75f, Colours::lightblue),
	down_button				("down"	, 0.25f, Colours::lightblue),
	apply_button			("Apply..."),
	cancel_button			("Cancel")
{
	setContentNonOwned		(&canvas, true	);
	setUsingNativeTitleBar	(true			);
	setResizable			(true, false	);
	setDropShadowEnabled	(true			);

	canvas.addAndMakeVisible(table);
	canvas.addAndMakeVisible(add_button);
	canvas.addAndMakeVisible(up_button);
	canvas.addAndMakeVisible(down_button);
	canvas.addAndMakeVisible(apply_button);
	canvas.addAndMakeVisible(cancel_button);

	table.getHeader().addColumn("original id"	, 1, 100, 30, -1, TableHeaderComponent::notSortable);
	table.getHeader().addColumn("original name"	, 2, 100, 30, -1, TableHeaderComponent::notSortable);
	table.getHeader().addColumn("images"		, 3, 100, 30, -1, TableHeaderComponent::notSortable);
	table.getHeader().addColumn("count"			, 4, 100, 30, -1, TableHeaderComponent::notSortable);
	table.getHeader().addColumn("action"		, 5, 100, 30, -1, TableHeaderComponent::notSortable);
	table.getHeader().addColumn("new id"		, 6, 100, 30, -1, TableHeaderComponent::notSortable);
	table.getHeader().addColumn("new name"		, 7, 100, 30, -1, TableHeaderComponent::notSortable);

	table.getHeader().setStretchToFitActive(true);
	table.getHeader().setPopupMenuActive(false);
	table.setModel(this);

	up_button		.setTooltip("Move the selected class up.");
	down_button		.setTooltip("Move the selected class down.");

	up_button		.setVisible(false);
	down_button		.setVisible(false);

	add_button		.addListener(this);
	up_button		.addListener(this);
	down_button		.addListener(this);
	apply_button	.addListener(this);
	cancel_button	.addListener(this);

	std::ifstream ifs(names_fn);
	std::string line;
	while (std::getline(ifs, line))
	{
		add_row(line);
	}

	setIcon(DarkMarkLogo());
	ComponentPeer *peer = getPeer();
	if (peer)
	{
		peer->setIcon(DarkMarkLogo());
	}

	if (cfg().containsKey("ClassIdWnd"))
	{
		restoreWindowStateFromString(cfg().getValue("ClassIdWnd"));
	}
	else
	{
		centreWithSize(640, 400);
	}

	rebuild_table();

	setVisible(true);

	for (const auto & info : vinfo)
	{
		count_files_per_class[info.original_id]			= 0;
		count_annotations_per_class[info.original_id]	= 0;
	}

	counting_thread = std::thread(&dm::ClassIdWnd::count_images_and_marks, this);

	return;
}


dm::ClassIdWnd::~ClassIdWnd()
{
	done = true;
	signalThreadShouldExit();
	cfg().setValue("ClassIdWnd", getWindowStateAsString());

	if (counting_thread.joinable())
	{
		counting_thread.join();
	}

	return;
}


void dm::ClassIdWnd::add_row(const std::string & name)
{
	Info info;
	info.original_id	= vinfo.size();
	info.original_name	= String(name).trim().toStdString();
	info.modified_name	= info.original_name;

	if (name == "new class")
	{
		info.original_id = -1;
	}

	vinfo.push_back(info);

	return;
}


void dm::ClassIdWnd::closeButtonPressed()
{
	setVisible(false);
	exitModalState(0);

	return;
}


void dm::ClassIdWnd::userTriedToCloseWindow()
{
	// ALT+F4

	closeButtonPressed();

	return;
}


void dm::ClassIdWnd::resized()
{
	// get the document window to resize the canvas, then we'll deal with the rest of the components
	DocumentWindow::resized();

	const int margin_size = 5;

	FlexBox button_row;
	button_row.flexDirection = FlexBox::Direction::row;
	button_row.justifyContent = FlexBox::JustifyContent::flexEnd;
	button_row.items.add(FlexItem(add_button)		.withWidth(100.0));
	button_row.items.add(FlexItem()					.withFlex(1.0));
	button_row.items.add(FlexItem(up_button)		.withWidth(50.0));
	button_row.items.add(FlexItem(down_button)		.withWidth(50.0));
	button_row.items.add(FlexItem()					.withFlex(1.0));
	button_row.items.add(FlexItem(apply_button)		.withWidth(100.0));
	button_row.items.add(FlexItem()					.withWidth(margin_size));
	button_row.items.add(FlexItem(cancel_button)	.withWidth(100.0));

	FlexBox fb;
	fb.flexDirection = FlexBox::Direction::column;
	fb.items.add(FlexItem(table).withFlex(1.0));
	fb.items.add(FlexItem(button_row).withHeight(30.0).withMargin(FlexItem::Margin(margin_size, 0, 0, 0)));

	auto r = getLocalBounds();
	r.reduce(margin_size, margin_size);
	fb.performLayout(r);

	return;
}


void dm::ClassIdWnd::buttonClicked(Button * button)
{
	if (button == &add_button)
	{
		add_row("new class");
		rebuild_table();
	}
	else if (button == &cancel_button)
	{
		closeButtonPressed();
	}
	else if (button == &up_button)
	{
		const int row = table.getSelectedRow();
		if (row > 0)
		{
			std::swap(vinfo[row], vinfo[row - 1]);
			rebuild_table();
			table.selectRow(row - 1);
		}
	}
	else if (button == &down_button)
	{
		const int row = table.getSelectedRow();
		if (row + 1 < (int)vinfo.size())
		{
			std::swap(vinfo[row], vinfo[row + 1]);
			rebuild_table();
			table.selectRow(row + 1);
		}
	}

	return;
}


void dm::ClassIdWnd::run()
{
}


int dm::ClassIdWnd::getNumRows()
{
	return vinfo.size();
}


void dm::ClassIdWnd::paintRowBackground(Graphics &g, int rowNumber, int width, int height, bool rowIsSelected)
{
	if (rowNumber < 0					or
		rowNumber >= (int)vinfo.size()	)
	{
		// rows are 0-based, columns are 1-based
		return;
	}

	Colour colour = Colours::white;
	if (rowIsSelected)
	{
		colour = Colours::lightblue; // selected rows will have a blue background
	}
	else if (vinfo[rowNumber].action == EAction::kDelete)
	{
		colour = Colours::darksalmon;
	}
	else
	{
		colour = Colours::lightgreen;
	}

	g.fillAll(colour);

	// draw the cell bottom divider between rows
	g.setColour( Colours::black.withAlpha(0.5f));
	g.drawLine(0, height, width, height);

	return;
}


void dm::ClassIdWnd::paintCell(Graphics & g, int rowNumber, int columnId, int width, int height, bool rowIsSelected)
{
	if (rowNumber < 0					or
		rowNumber >= (int)vinfo.size()	or
		columnId < 1					or
		columnId > 7					)
	{
		// rows are 0-based, columns are 1-based
		return;
	}

	const auto & info = vinfo[rowNumber];

	String str;
	Justification justification = Justification::centredLeft;

	if (columnId == 1) // original ID
	{
		str = String(info.original_id);
	}
	else if (columnId == 2) // original name
	{
		str = info.original_name;
	}
	else if (columnId == 3) // images
	{
		if (count_files_per_class.count(info.original_id))
		{
			str = String(count_files_per_class.at(info.original_id));
			justification = Justification::centredRight;
		}
	}
	else if (columnId == 4) // counter
	{
		if (count_annotations_per_class.count(info.original_id))
		{
			str = String(count_annotations_per_class.at(info.original_id));
			justification = Justification::centredRight;
		}
	}
	else if (columnId == 5) // action
	{
		switch (info.action)
		{
			case EAction::kMerge:
			{
				str = "merged";
				break;
			}
			case EAction::kDelete:
			{
				str = "deleted";
				break;
			}
			case EAction::kNone:
			{
				if (info.original_name != info.modified_name)
				{
					str = "renamed";
				}
				else if (info.original_id != info.modified_id)
				{
					str = "reordered";
				}
				break;
			}
			default:
			{
				break;
			}
		}
	}
	else if (columnId == 6) // modified ID
	{
		if (info.action != EAction::kDelete)
		{
			str = String(info.modified_id);
		}
	}
	else if (columnId == 7) // modified name
	{
		if (info.action != EAction::kDelete)
		{
			str = info.modified_name;
		}
	}

	// draw the text and the right-hand-side dividing line between cells
	if (str.isNotEmpty())
	{
		g.setColour(Colours::black);
		Rectangle<int> r(0, 0, width, height);
		g.drawFittedText(str, r.reduced(2), justification, 1 );
	}

	// draw the divider on the right side of the column
	g.setColour(Colours::black.withAlpha(0.5f));
	g.drawLine(width, 0, width, height);

	return;
}


void dm::ClassIdWnd::selectedRowsChanged(int rowNumber)
{
	if (rowNumber < 0					or
		rowNumber >= (int)vinfo.size()	)
	{
		// rows are 0-based, columns are 1-based
		return;
	}

	up_button	.setVisible(vinfo.size() > 1 and rowNumber >= 1);
	down_button	.setVisible(vinfo.size() > 1 and rowNumber + 1 < (int)vinfo.size());

	table.repaint();

	return;
}


void dm::ClassIdWnd::cellClicked(int rowNumber, int columnId, const MouseEvent & event)
{
	if (rowNumber < 0					or
		rowNumber >= (int)vinfo.size())
	{
		// rows are 0-based, columns are 1-based
		return;
	}

	if (columnId == 5 or columnId == 6) // "action" or "new id"
	{
		auto & info = vinfo[rowNumber];
		std::string name = info.modified_name;
		if (name.empty()) // for example, a deleted class has no name
		{
			name = info.original_name;
		}
		if (name.empty()) // a newly-added row might not yet have a name
		{
			name = "#" + std::to_string(info.original_id);
		}

		PopupMenu m;
		m.addSectionHeader("\"" + name + "\"");
		m.addSeparator();

		// is there another class that is supposed to merge to this one?
		bool found_a_class_that_merges_to_this_one = false;
		for (size_t idx = 0; idx < vinfo.size(); idx ++)
		{
			if (rowNumber == (int)idx)
			{
				continue;
			}

			if (vinfo[idx].action == EAction::kMerge and
				vinfo[idx].modified_id == info.modified_id)
			{
				found_a_class_that_merges_to_this_one = true;
				break;
			}
		}

		if (found_a_class_that_merges_to_this_one)
		{
			m.addItem("cannot delete this class due to pending merge", false, false, std::function<void()>( []{return;} ));
		}
		else if (info.action == EAction::kDelete)
		{
			// this will clear the deletion flag
			m.addItem("delete this class"	, true, true, std::function<void()>( [=]() { this->vinfo[rowNumber].action = EAction::kNone; } ));
		}
		else
		{
			// this will set the deletion flag
			m.addItem("delete this class"	, true, false, std::function<void()>( [=]() { this->vinfo[rowNumber].action = EAction::kDelete; } ));
		}

		if (vinfo.size() > 1)
		{
			m.addSeparator();

			if (found_a_class_that_merges_to_this_one)
			{
				m.addItem("cannot merge this class due to pending merge", false, false, std::function<void()>( []{return;} ));
			}
			else
			{
				PopupMenu merged;

				for (size_t idx = 0; idx < vinfo.size(); idx ++)
				{
					if (rowNumber == (int)idx)
					{
						continue;
					}

					const auto & dst = vinfo[idx];

					if (dst.action != EAction::kNone)
					{
						// cannot merge to a class that is deleted or already merged
						continue;
					}

					if (info.action == EAction::kMerge and info.merge_to_name == dst.modified_name)
					{
						merged.addItem("merge with class #" + std::to_string(dst.modified_id) + " \"" + info.merge_to_name + "\"", true, true, std::function<void()>(
							[=]()
							{
								// un-merge
								this->vinfo[rowNumber].action = EAction::kNone;
								this->vinfo[rowNumber].merge_to_name.clear();
								this->vinfo[rowNumber].modified_name = this->vinfo[rowNumber].original_name;
								return;
							}));
					}
					else
					{
						merged.addItem("merge with class #" + std::to_string(dst.modified_id) + " \"" + dst.modified_name + "\"", true, false, std::function<void()>(
							[=]()
							{
								// merge
								this->vinfo[rowNumber].action = EAction::kMerge;
								this->vinfo[rowNumber].merge_to_name = dst.modified_name;
								this->vinfo[rowNumber].modified_name = dst.modified_name;
								return;
							}));
					}
				}
				m.addSubMenu("merge", merged);
			}
		}

		m.show();
		rebuild_table();
	}

	return;
}


Component * dm::ClassIdWnd::refreshComponentForCell(int rowNumber, int columnId, bool isRowSelected, Component * existingComponentToUpdate)
{
	// remove whatever previously existed for a different row,
	// we'll create a new one specific to this row if needed
	delete existingComponentToUpdate;
	existingComponentToUpdate = nullptr;

	// rows are 0-based, columns are 1-based
	// column #7 is where they get to type a new name for the class
	if (rowNumber >= 0					and
		rowNumber < (int)vinfo.size()	and
		columnId == 7					and
		isRowSelected					and
		vinfo[rowNumber].action == EAction::kNone)
	{
		auto editor = new TextEditor("class name editor");
		editor->setColour(TextEditor::ColourIds::backgroundColourId, Colours::lightblue);
		editor->setColour(TextEditor::ColourIds::textColourId, Colours::black);
		editor->setColour(TextEditor::ColourIds::highlightedTextColourId, Colours::black);
		editor->setMouseClickGrabsKeyboardFocus(true);
		editor->setEscapeAndReturnKeysConsumed(false);
		editor->setReturnKeyStartsNewLine(false);
		editor->setTabKeyUsedAsCharacter(false);
		editor->setSelectAllWhenFocused(true);
		editor->setWantsKeyboardFocus(true);
		editor->setPopupMenuEnabled(false);
		editor->setScrollbarsShown(false);
		editor->setCaretVisible(true);
		editor->setMultiLine(false);
		editor->setText(vinfo[rowNumber].modified_name);
		editor->moveCaretToEnd();
		editor->selectAll();

		editor->onEscapeKey = [=]
		{
			// go back to the previous name
			editor->setText(vinfo[rowNumber].modified_name);
			this->rebuild_table();
		};
		editor->onFocusLost = [=]
		{
			auto str = editor->getText().trim().toStdString();
			if (str.empty())
			{
				str = this->vinfo[rowNumber].modified_name;
				if (str.empty())
				{
					str = this->vinfo[rowNumber].original_name;
				}
			}

			this->vinfo[rowNumber].modified_name = str;

			// see if there are other classes which are merged here, in which case we need to update their names
			for (size_t idx = 0; idx < this->vinfo.size(); idx ++)
			{
				auto & dst = this->vinfo[idx];
				if (dst.action == EAction::kMerge and
					dst.modified_id == this->vinfo[rowNumber].modified_id)
				{
					dst.merge_to_name = str;
					dst.modified_name = str;
				}
			}

			this->rebuild_table();
		};
		editor->onReturnKey = editor->onFocusLost;

		existingComponentToUpdate = editor;
	}

	return existingComponentToUpdate;
}


String dm::ClassIdWnd::getCellTooltip(int rowNumber, int columnId)
{
	String str = "Click on \"action\" or \"new id\" columns to delete or merge.";

	if (error_count > 0)
	{
		str = "Errors detected while processing annotations.  See log file for details.";
	}
	else if (columnId == 3)
	{
		str = "The number of .txt files which reference this class ID.";
	}
	else if (columnId == 4)
	{
		str = "The total number of annotations for this class ID.";
	}

	return str;
}


void dm::ClassIdWnd::rebuild_table()
{
	dm::Log("rebuilding table");

	int next_class_id = 0;

	// assign the ID to be used by each class
	for (size_t idx = 0; idx < vinfo.size(); idx ++)
	{
		auto & info = vinfo[idx];

		if (info.action == EAction::kDelete)
		{
			dm::Log("class \"" + info.modified_name + "\" has been deleted");
			info.modified_id = -1;
		}
		else if (info.action == EAction::kMerge)
		{
			dm::Log("class \"" + info.original_name + "\" has been merged to \"" + info.merge_to_name + "\", will need to revisit once IDs have been assigned");
			info.modified_id = -1;
		}
		else
		{
			dm::Log("class \"" + info.modified_name + "\" has been assigned id #" + std::to_string(next_class_id));
			info.modified_id = next_class_id ++;
		}
	}

	// now that IDs have been assigned we need to look at merge entries and figure out the right ID to use
	for (size_t idx = 0; idx < vinfo.size(); idx ++)
	{
		auto & info = vinfo[idx];

		if (info.action == EAction::kMerge)
		{
			// look for the non-merge entry that has this name
			for (size_t tmp = 0; tmp < vinfo.size(); tmp ++)
			{
				if (vinfo[tmp].action != EAction::kMerge and vinfo[tmp].modified_name == info.merge_to_name)
				{
					// we found the item we need to merge to!
					info.modified_id = vinfo[tmp].modified_id;
					dm::Log("class \"" + info.original_name + "\" is merged with #" + std::to_string(info.modified_id) + " \"" + info.modified_name + "\"");
					break;
				}
			}
		}
	}

	// see if we have any changes to apply
	bool changes_to_apply = false;
	for (size_t idx = 0; idx < vinfo.size(); idx ++)
	{
		auto & info = vinfo[idx];
		if (info.original_id != info.modified_id or
			info.original_name != info.modified_name)
		{
			changes_to_apply = true;
			break;
		}
	}

	apply_button.setEnabled(error_count == 0 and changes_to_apply);

	table.updateContent();
	table.repaint();

	return;
}


void dm::ClassIdWnd::count_images_and_marks()
{
	// this is started on a secondary thread

	try
	{
		error_count = 0;

		VStr image_filenames;
		VStr json_filenames;
		VStr images_without_json;

		find_files(dir, image_filenames, json_filenames, images_without_json, done);

		dm::Log("counting thread: number of images found in " + dir.getFullPathName().toStdString() + ": " + std::to_string(image_filenames.size()));

		for (size_t idx = 0; idx < image_filenames.size() and not threadShouldExit(); idx ++)
		{
			auto & fn = image_filenames[idx];
			File file = File(fn).withFileExtension(".txt");
			if (file.exists())
			{
				std::set<int> classes_found;

				std::ifstream ifs(file.getFullPathName().toStdString());
				while (not threadShouldExit())
				{
					int class_id = -1;
					float x = -1.0f;
					float y = -1.0f;
					float w = -1.0f;
					float h = -1.0f;

					ifs >> class_id >> x >> y >> w >> h;

					if (not ifs.good())
					{
						break;
					}

					if (class_id < 0 or class_id >= (int)vinfo.size())
					{
						error_count ++;
						dm::Log("ERROR: class #" + std::to_string(class_id) + " in " + file.getFullPathName().toStdString() + " is invalid");
					}
					else if (
						x <= 0.0f or
						y <= 0.0f or
						w <= 0.0f or
						h <= 0.0f)
					{
						error_count ++;
						dm::Log("ERROR: coordinates for class #" + std::to_string(class_id) + " in " + file.getFullPathName().toStdString() + " are invalid");
					}
					else if (
						x - w / 2.0f < 0.0f or
						x + w / 2.0f > 1.0f or
						y - h / 2.0f < 0.0f or
						y + h / 2.0f > 1.0f)
					{
						error_count ++;
						dm::Log("ERROR: width or height for class #" + std::to_string(class_id) + " in " + file.getFullPathName().toStdString() + " is invalid");
					}
					else
					{
						classes_found.insert(class_id);
						count_annotations_per_class[class_id] ++;
					}
				}

				for (const int id : classes_found)
				{
					count_files_per_class[id] ++;
				}
			}
		}

		if (error_count)
		{
			apply_button.setTooltip("Cannot apply changes due to errors.  See log file for details.");
		}
	}
	catch(const std::exception & e)
	{
		dm::Log(std::string("counting thread exception: ") + e.what());
	}

	Log("counting thread is exiting");

	return;
}
