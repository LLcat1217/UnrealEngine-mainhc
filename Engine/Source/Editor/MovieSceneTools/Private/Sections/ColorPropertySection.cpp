// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneToolsPrivatePCH.h"
#include "MovieSceneColorTrack.h"
#include "MovieSceneColorSection.h"
#include "ColorPropertySection.h"
#include "MovieSceneSequence.h"

void FColorPropertySection::GenerateSectionLayout( class ISectionLayoutBuilder& LayoutBuilder ) const
{
	UMovieSceneColorSection* ColorSection = Cast<UMovieSceneColorSection>( &SectionObject );

	LayoutBuilder.AddKeyArea( "R", NSLOCTEXT( "FColorPropertySection", "RedArea", "Red" ), MakeShareable( new FFloatCurveKeyArea( &ColorSection->GetRedCurve(), ColorSection ) ) );
	LayoutBuilder.AddKeyArea( "G", NSLOCTEXT( "FColorPropertySection", "GreenArea", "Green" ), MakeShareable( new FFloatCurveKeyArea( &ColorSection->GetGreenCurve(), ColorSection ) ) );
	LayoutBuilder.AddKeyArea( "B", NSLOCTEXT( "FColorPropertySection", "BlueArea", "Blue" ), MakeShareable( new FFloatCurveKeyArea( &ColorSection->GetBlueCurve(), ColorSection ) ) );
	LayoutBuilder.AddKeyArea( "A", NSLOCTEXT( "FColorPropertySection", "OpacityArea", "Opacity" ), MakeShareable( new FFloatCurveKeyArea( &ColorSection->GetAlphaCurve(), ColorSection ) ) );
}

int32 FColorPropertySection::OnPaintSection( const FGeometry& AllottedGeometry, const FSlateRect& SectionClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, bool bParentEnabled ) const
{
	const UMovieSceneColorSection* ColorSection = Cast<const UMovieSceneColorSection>( &SectionObject );

	float StartTime = ColorSection->GetStartTime();
	float EndTime = ColorSection->GetEndTime();
	float SectionDuration = EndTime - StartTime;

	if ( !FMath::IsNearlyZero( SectionDuration ) )
	{
		LayerId = FPropertySection::OnPaintSection( AllottedGeometry, SectionClippingRect, OutDrawElements, LayerId, bParentEnabled );

		FVector2D GradientSize = FVector2D( AllottedGeometry.Size.X, (AllottedGeometry.Size.Y / 4) - 3.0f );

		FPaintGeometry PaintGeometry = AllottedGeometry.ToPaintGeometry( FVector2D( 0, 0 ), GradientSize );

		// If we are showing a background pattern and the colors is transparent, draw a checker pattern
		const FSlateBrush* CheckerBrush = FEditorStyle::GetBrush( "Checker" );
		FSlateDrawElement::MakeBox( OutDrawElements, LayerId, PaintGeometry, CheckerBrush, SectionClippingRect );

		TArray<FSlateGradientStop> GradientStops;

		TArray< TKeyValuePair<float, FLinearColor> > ColorKeys;
		ConsolidateColorCurves( ColorKeys, ColorSection );

		for ( int32 i = 0; i < ColorKeys.Num(); ++i )
		{
			float Time = ColorKeys[i].Key;
			FLinearColor Color = ColorKeys[i].Value;
			float TimeFraction = (Time - StartTime) / SectionDuration;

			GradientStops.Add( FSlateGradientStop( FVector2D( TimeFraction * AllottedGeometry.Size.X, 0 ),
				Color ) );
		}

		if ( GradientStops.Num() > 0 )
		{
			FSlateDrawElement::MakeGradient(
				OutDrawElements,
				LayerId + 1,
				PaintGeometry,
				GradientStops,
				Orient_Vertical,
				SectionClippingRect
				);
		}
	}

	return LayerId + 1;
}


void FColorPropertySection::ConsolidateColorCurves( TArray< TKeyValuePair<float, FLinearColor> >& OutColorKeys, const UMovieSceneColorSection* Section ) const
{
	// Get the default color of the first instance
	FLinearColor DefaultColor( 0.0f, 0.0f, 0.0f, 0.0f );

	static const FName SlateColorName( "SlateColor" );

	UMovieSceneSequence* Sequence = Sequencer->GetFocusedMovieSceneSequence();

	const TArray<FMovieSceneBinding>& MovieSceneBindings = Sequence->GetMovieScene()->GetBindings();

	bool bFoundColor = false;
	for ( int32 BindingIndex = 0; BindingIndex < MovieSceneBindings.Num() && !bFoundColor; ++BindingIndex )
	{
		const FMovieSceneBinding& MovieSceneBinding = MovieSceneBindings[BindingIndex];

		for ( int32 TrackIndex = 0; TrackIndex < MovieSceneBinding.GetTracks().Num() && !bFoundColor; ++TrackIndex )
		{
			if ( MovieSceneBinding.GetTracks()[TrackIndex] == Track )
			{
				UObject* RuntimeObject = Sequence->FindObject( MovieSceneBinding.GetObjectGuid() );

				if ( RuntimeObject != nullptr )
				{
					UProperty* Property = RuntimeObject->GetClass()->FindPropertyByName( CastChecked<UMovieSceneColorTrack>( Track )->GetPropertyName() );
					UStructProperty* ColorStructProp = Cast<UStructProperty>( Property );
					if ( ColorStructProp && ColorStructProp->Struct )
					{
						if ( ColorStructProp->Struct->GetFName() == SlateColorName )
						{
							DefaultColor = (*Property->ContainerPtrToValuePtr<FSlateColor>( RuntimeObject )).GetSpecifiedColor();
						}
						else if ( ColorStructProp->Struct->GetFName() == NAME_LinearColor )
						{
							DefaultColor = *Property->ContainerPtrToValuePtr<FLinearColor>( RuntimeObject );
						}
						else
						{
							DefaultColor = Property->ContainerPtrToValuePtr<FColor>( RuntimeObject )->ReinterpretAsLinear();
						}
						bFoundColor = true;
						break;
					}
				}
			}
		}
	}

	// @todo Sequencer Optimize - This could all get cached, instead of recalculating everything every OnPaint

	const FRichCurve* Curves[4] = {
		&Section->GetRedCurve(),
		&Section->GetGreenCurve(),
		&Section->GetBlueCurve(),
		&Section->GetAlphaCurve()
	};

	// @todo Sequencer Optimize - This is a O(n^2) loop!
	// Our times are floats, which means we can't use a map and
	// do a quick lookup to see if the keys already exist
	// because the keys are ordered, we could take advantage of that, however
	TArray<float> TimesWithKeys;
	for ( int32 i = 0; i < 4; ++i )
	{
		const FRichCurve* Curve = Curves[i];
		for ( auto It( Curve->GetKeyIterator() ); It; ++It )
		{
			float KeyTime = It->Time;

			bool bShouldAddKey = true;

			int32 InsertKeyIndex = INDEX_NONE;
			for ( int32 k = 0; k < TimesWithKeys.Num(); ++k )
			{
				if ( FMath::IsNearlyEqual( TimesWithKeys[k], KeyTime ) )
				{
					bShouldAddKey = false;
					break;
				}
				else if ( TimesWithKeys[k] > KeyTime )
				{
					InsertKeyIndex = k;
					break;
				}
			}

			if ( InsertKeyIndex == INDEX_NONE && bShouldAddKey )
			{
				InsertKeyIndex = TimesWithKeys.Num();
			}

			if ( bShouldAddKey )
			{
				TimesWithKeys.Insert( KeyTime, InsertKeyIndex );
			}
		}
	}

	// @todo Sequencer Optimize - This another O(n^2) loop, since Eval is O(n)!
	for ( int32 i = 0; i < TimesWithKeys.Num(); ++i )
	{
		OutColorKeys.Add( TKeyValuePair<float, FLinearColor>( TimesWithKeys[i], Section->Eval( TimesWithKeys[i], DefaultColor ) ) );
	}
}